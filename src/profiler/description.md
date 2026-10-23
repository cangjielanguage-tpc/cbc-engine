Producer — исполняющийся файбер, который записывает вызовы функций.
Consumer — отдельный поток-коллектор, который периодически обрабатывает профильную информацию.

Файбер может мигрировать между OS-потоками. Алгоритм предполагает, что в каждый момент времени один `FiberProfile` использует только один Producer, а передача файбера между OS-потоками обеспечивает необходимую синхронизацию памяти.
Consumer также существует в одном экземпляре.

## Основная идея

У каждого файбера есть два буфера. В каждый момент один из них является текущим writer-буфером Producer, а второй может быть доступен Consumer. Если Consumer ничего не читает, после заполнения текущего буфера Producer переключается на следующий, чтобы поддерживать актуальность профильной информации. Поэтому при обычной работе Producer попеременно заполняет буферы `0` и `1`, а Consumer читает буфер, противоположный текущему writer-буферу.

При заполнении текущего буфера Producer временно переводит состояние читателя в `TRANS`:

- если Consumer ничего не читает (`NO`), Producer переключается на второй буфер, а заполненный буфер становится доступен Consumer;
- если Consumer читает (`YES`), Producer временно не переключается на занятый Consumer буфер и циклически перезаписывает текущий writer-буфер. После завершения чтения переключение снова становится возможным при следующем заполнении.

Таким образом, Producer никогда не начинает запись в буфер, который обрабатывает Consumer. При этом блокировки случаются не на каждом вызове `RecordCall`: протокол смены буфера выполняется только один раз на 4096 записей.

Профилировщик хранит статистику только одного прохода Consumer. Счётчики разных файберов объединяются в рамках этого прохода, но перед следующим проходом создаются заново. Поэтому один опубликованный буфер разрешено читать повторно.

## Структуры данных
```cpp
class FiberProfile {
private:
    static constexpr uint32_t CALL_BUFFER_SIZE = 4096;
    static constexpr uint32_t NO_PUBLISHED_BUFFER = 2;

    struct CallRecord {
        FunctionId caller;
        CallSiteId callSite;
    };

    struct CallBuffer {
        std::array<CallRecord, CALL_BUFFER_SIZE> records;
    };

    enum class ReaderState : uint8_t {
        NO,
        YES,
        TRANS
    };

    std::array<CallBuffer, 2> buffers;

    uint32_t writerIndex { 0 };
    uint32_t cursor { 0 };
    uint32_t count { 0 };

    std::atomic<uint32_t> sharedWriterIndex { NO_PUBLISHED_BUFFER };
    std::atomic<ReaderState> readerState { ReaderState::NO };
    std::atomic<bool> active { true };
};
```

- `CallRecord` содержит идентификатор вызывающей функции и абсолютный адрес инструкции вызова в её байткоде.
- `buffers` — два массива записей одного файбера.
- `writerIndex` — producer-local индекс текущего буфера записи: `0` или `1`.
- `cursor` — позиция следующей записи в текущем буфере.
- `count` — количество действительных записей в текущем буфере, не больше `CALL_BUFFER_SIZE`.
- `sharedWriterIndex` — атомарно опубликованный индекс текущего writer-буфера. Consumer читает буфер с противоположным индексом.
- `NO_PUBLISHED_BUFFER == 2` — начальное значение `sharedWriterIndex`. Индексы `0` и `1` являются допустимыми, поэтому `2` означает, что Producer ещё ни разу не заполнил и не опубликовал буфер.
- `readerState` координирует снимок Consumer и переключение Producer.
- `active` показывает, может ли Producer ещё обращаться к профилю.

`writerIndex`, `cursor` и `count` изменяет только Producer. Пока профиль активен, Consumer не читает эти поля: для выбора буфера он использует отдельный атомарный `sharedWriterIndex`. Прямое чтение producer-local полей разрешено лишь после завершения файбера.

## Состояния читателя

`readerState` принимает три значения:

1. `NO` — Consumer не читает буфер. Producer может переключить writer-буфер.
2. `YES` — Consumer зафиксировал снимок и читает противоположный буфер. Producer продолжает писать в текущий буфер.
3. `TRANS` — Producer атомарно решает, можно ли переключить буфер. Пока активно это состояние, Consumer не может начать или завершить свой переход.

Основной инвариант:

```text
readerIndex != writerIndex
```

Пока Consumer находится в состоянии `YES`, значение writer-индекса не может
измениться. Поэтому Producer и Consumer не обращаются к одной записи буфера
одновременно.

## Запись события

```cpp
void RecordCall(FunctionId caller, CallSiteId callSite)
{
    auto& current = buffers[writerIndex];
    current.records[cursor] = CallRecord { caller, callSite };

    cursor = (cursor + 1) & (CALL_BUFFER_SIZE - 1);
    if (count < CALL_BUFFER_SIZE) {
        ++count;
    }

    if (cursor == 0) {
        TryPublish();
    }
}
```

На обычном вызове выполняются только:

1. запись пары `(caller, callSite)`;
2. обновление циклического индекса;
3. обновление локального счётчика записей.

Атомарных операций в обычном `RecordCall` нет. `TryPublish` вызывается только после полного оборота по буферу.

Если переключиться не получилось, `cursor` уже равен нулю, `count` остаётся равным
размеру буфера, и Producer начинает перезаписывать самые старые записи текущего
буфера.

## Публикация и смена буфера

```cpp
void FiberProfile::TryPublish()
{
    auto previous = readerState.load(std::memory_order_relaxed);
    while (previous == ReaderState::TRANS ||
           !readerState.compare_exchange_weak(
               previous,
               ReaderState::TRANS,
               std::memory_order_acq_rel,
               std::memory_order_relaxed
           )) {
        if (previous == ReaderState::TRANS) {
            std::this_thread::yield();
            previous = readerState.load(std::memory_order_relaxed);
        }
    }

    if (previous == ReaderState::NO) {
        writerIndex ^= 1U;
        cursor = 0;
        count = 0;
        sharedWriterIndex.store(writerIndex, std::memory_order_release);
    }

    readerState.store(previous, std::memory_order_release);
}
```

Producer выполняет следующие действия:

1. С помощью CAS заменяет текущее состояние `NO` или `YES` на `TRANS` и запоминает предыдущее значение.
2. Если предыдущее состояние было `NO`, переключает `writerIndex` и публикует его через `sharedWriterIndex`.
3. Если предыдущее состояние было `YES`, не переключает буфер: противоположный буфер в этот момент принадлежит Consumer.
4. Восстанавливает предыдущее состояние `NO` или `YES`.

`sharedWriterIndex` содержит индекс нового writer-буфера. Буфер Consumer вычисляется как `sharedWriterIndex ^ 1`.

## Чтение опубликованного буфера

### Захват и освобождение права чтения

Consumer не может просто записать в `readerState` значение `YES` или `NO`:
между загрузкой состояния и записью Producer мог перевести его в `TRANS`.
Обычный `store` тогда затёр бы состояние Producer и позволил бы одновременно
переключить и читать один буфер.

Поэтому Consumer меняет состояние только условно, с помощью
`ChangeReaderState`. Функция выполняет переход `from -> to`, только если в
момент CAS атомарная переменная всё ещё равна `from`:

```cpp
void ChangeReaderState(
    FiberProfile& profile,
    ReaderState from,
    ReaderState to,
    std::memory_order order
)
{
    while (true) {
        auto expected = from;
        if (profile.readerState.compare_exchange_weak(
                expected, to, order, std::memory_order_relaxed)) {
            return;
        }
        std::this_thread::yield();
    }
}
```

Функция используется для двух разных действий:

- `NO -> YES` — Consumer получает право читать опубликованный буфер;
- `YES -> NO` — Consumer сообщает, что чтение закончено и буфер снова можно
  использовать при следующем переключении Producer.

1. `expected` инициализируется значением `from`.
2. `compare_exchange_weak` атомарно сравнивает `readerState` с `expected`.
3. Если они равны, CAS записывает `to`. Consumer получил требуемое состояние, и функция завершается.
4. Если они не равны или произошёл допустимый для weak CAS spurious failure, операция завершается неудачей. Consumer вызывает `yield` и повторяет попытку.

`std::this_thread::yield()` позволяет планировщику запустить Producer, который должен закончить короткий участок `TRANS` и восстановить `NO` либо `YES`.

Memory order передаётся параметром:

- для `NO -> YES` используется `acquire`, чтобы последующие чтения Consumer не перемещались до получения права чтения;
- для `YES -> NO` используется `release`, чтобы завершённые чтения буфера были видимы до его освобождения;
- для неудачного CAS достаточно `relaxed`, потому что Consumer не получил требуемое состояние и не обращается к буферу, а только повторяет попытку.

Если при начале чтения Producer уже находится в `TRANS`, переход `NO -> YES` не срабатывает. Consumer ждёт, пока Producer закончит публикацию и восстановит  `NO`. Аналогично, если Producer успел выполнить `YES -> TRANS` перед окончанием чтения, переход Consumer `YES -> NO` ждёт восстановления `YES`. Это не позволяет Consumer ни войти в секцию чтения, ни выйти из неё посередине смены буфера.

### Выбор и обработка буфера

После успешного перехода `NO -> YES` чтение выполняется следующим образом:

```cpp
void ConsumePublished(FiberProfile& profile, CallCounters& counters)
{
    ChangeReaderState(profile, ReaderState::NO, ReaderState::YES,
                      std::memory_order_acquire);

    const auto writerIndex =
        profile.sharedWriterIndex.load(std::memory_order_acquire);

    if (writerIndex != FiberProfile::NO_PUBLISHED_BUFFER) {
        const auto readerIndex = writerIndex ^ 1U;
        ProcessBuffer(profile.buffers[readerIndex], 0,
                      FiberProfile::CALL_BUFFER_SIZE, counters);
    }

    ChangeReaderState(profile, ReaderState::YES, ReaderState::NO,
                      std::memory_order_release);
}
```

1. Consumer выполняет переход `NO -> YES`. Если Producer находится в `TRANS`, Consumer ждёт.
2. Загружает `sharedWriterIndex`.
3. До первой публикации видит `NO_PUBLISHED_BUFFER` и ничего не читает.
4. После публикации читает буфер `sharedWriterIndex ^ 1`.
5. После обработки выполняет переход `YES -> NO`. Если Producer временно заменил `YES` на `TRANS`, Consumer ждёт, пока тот восстановит `YES`.

Один и тот же буфер может обрабатываться в нескольких последовательных проходах. Это допустимо, потому что статистика предыдущего прохода не сохраняется.

## Memory ordering и корректность

Заполнение буфера происходит до:

```cpp
sharedWriterIndex.store(writerIndex, std::memory_order_release);
```

Consumer загружает индекс с `memory_order_acquire`. Поэтому записи заполненного буфера happens-before их чтения Consumer.

Consumer завершает чтение до release-перехода `YES -> NO`. Следующий успешный CAS Producer использует `memory_order_acq_rel`. Поэтому Producer не начинает повторное использование освобождённого буфера до завершения чтения.

Разбор гонок при захвате состояния:

- если Consumer первым выполнил `NO -> YES`, Producer увидит предыдущее состояние `YES` и не переключит буфер;
- если Producer первым выполнил `NO -> TRANS`, Consumer не сможет начать чтение, пока Producer не переключит буфер и не восстановит `NO`;
- если Producer временно выполнил `YES -> TRANS`, Consumer не сможет завершить переход `YES -> NO`, пока Producer не восстановит `YES`.

Алгоритм не использует мьютекс на пути записи события. Однако это не строгая lock-free гарантия прогресса: поток, остановленный внутри состояния `TRANS`, может временно задержать Consumer.

## Сбор статистики одного прохода

В начале каждого `ConsumeActiveProfiles` создаётся новая hashmap:

```cpp
CallCounters counters;
```

В неё добавляются записи последних доступных снимков всех активных профилей:

```cpp
++counters[CallEdge { record.caller, record.callSite }];
```

Таким образом, одинаковые рёбра вызовов из разных файберов суммируются, но результаты предыдущего прохода не влияют на следующий. После обхода значения преобразуются в `ProfiledCall` и передаются в `LogProfilerPass`.

Для прямого вызова Producer сохраняет `caller` и абсолютный адрес инструкции `callSite`. Consumer получает байткод из `DynamicFunctionHandle`, декодирует `DIRECT_CALL_2I`, извлекает `callee` из таблицы literals и разрешает имена обеих функций. Вызов считается горячим, если его счётчик в текущем проходе не меньше `HOT_CALL_COUNT`.

## Завершение файбера

```cpp
void DestroyFiberProfile(FiberProfileHandle& profile)
{
    if (profile) {
        profile->active.store(false, std::memory_order_release);
        profile.reset();
    }
}
```

Consumer загружает `active` с `memory_order_acquire`. Если Producer завершён, он больше не изменяет producer-local поля и текущий буфер. Consumer сначала обрабатывает опубликованный полный буфер, а затем безопасно читает оставшиеся `count` записей текущего, возможно частично заполненного буфера. После этого профиль удаляется из списка активных.

## Верификация с помощью CDSChecker

Протокол состояний и буферов проверялся с помощью CDSChecker — model checker для атомарных операций и memory model C11/C++11.

### Устройство модели

Модель сохраняет существенные части алгоритма:

- два буфера;
- одного Producer и одного Consumer;
- состояния `NO`, `YES`, `TRANS`;
- те же CAS-переходы и memory orders;
- `sharedWriterIndex` со значением `NO_PUBLISHED_BUFFER` до первой публикации;
- переключение только при предыдущем состоянии `NO`.

Чтобы перебрать все состояния, размер буфера уменьшён до одного. Producer выполняет три заполнения: этого достаточно, чтобы повторно использовать первый буфер и обнаружить возможное одновременное чтение и запись. Потоки создаются через C11 `thrd_create`. В критических местах вызывается `thrd_yield`, чтобы model checker исследовал больше вариантов взаимного расположения операций.

Схема модели выглядит так:

```cpp
void Producer(void*)
{
    for (uint32_t batch = 1; batch <= 3; ++batch) {
        for (uint32_t i = 0; i < BUFFER_SIZE; ++i) {
            store_32(&buffers[writerIndex][cursor], batch);
            cursor = (cursor + 1U) & (BUFFER_SIZE - 1U);
        }
        TryPublish();
    }
}

void Consumer(void*)
{
    // CAS: NO -> YES
    int expected = NO;
    while (!readerState.compare_exchange_weak(
        expected, YES, std::memory_order_acquire, std::memory_order_relaxed
    )) {
        expected = NO;
        thrd_yield();
    }

    const auto currentSharedWriterIndex = sharedWriterIndex.load(std::memory_order_acquire);
    if (currentSharedWriterIndex != NO_PUBLISHED_BUFFER) {
        const auto readerIndex = currentSharedWriterIndex ^ 1U;
        MODEL_ASSERT(readerIndex != currentSharedWriterIndex);

        thrd_yield();

        const auto currentWriterIndex = static_cast<uint32_t>(
            sharedWriterIndex.load(std::memory_order_relaxed)
        );
        MODEL_ASSERT(readerIndex != currentWriterIndex);

        const auto record = load_32(&buffers[readerIndex][0]);

        MODEL_ASSERT(record != 0);
    }

    // CAS: YES -> NO
    expected = YES;
    while (!readerState.compare_exchange_weak(
        expected, NO, std::memory_order_release, std::memory_order_relaxed
    )) {
        expected = YES;
        thrd_yield();
    }
}
```

### Проверяемые свойства

Обычные обращения к записям заменены функциями `store_32` и `load_32` из `librace`. Благодаря этому CDSChecker автоматически сообщает о data race, если Producer и Consumer обращаются к одной записи без happens-before.

В модели установлены следующие assertions:

```cpp
MODEL_ASSERT(readerIndex != currentSharedWriterIndex);
```

Проверяет, что выбранный Consumer буфер отличается от writer-буфера в момент получения снимка.

```cpp
thrd_yield();
const auto currentWriterIndex = sharedWriterIndex.load(...);
MODEL_ASSERT(readerIndex != currentWriterIndex);
```

Проверяет после возможного переключения планировщика, что Producer не смог сменить индекс на буфер Consumer во время чтения.

```cpp
MODEL_ASSERT(record != 0);
```

Проверяет, что Consumer не прочитал буфер до его первой полной инициализации.

Также проверяется отсутствие deadlock в конечной модели. При намеренном нарушении протокола — если разрешить Producer переключать буфер при предыдущем состоянии `YES` — CDSChecker обнаруживает data race между `load_32` Consumer и `store_32` Producer. Это использовалось как отрицательная проверка, что модель действительно чувствительна к нарушению основного инварианта.

Для корректной версии итог model checking содержит:

```text
Number of complete, bug-free executions: 872
Number of buggy executions: 0
Total executions: 3159
```

### Границы проверки

Модель проверяет safety-свойства уменьшенного протокола при следующих предположениях:

1. Для одного `FiberProfile` одновременно существует только один Producer.
2. Consumer существует в одном экземпляре.
3. После `DestroyFiberProfile` больше не вызывается `RecordCall`.
4. Миграция файбера между OS-потоками сама корректно передаёт владение и видимость producer-local данных.
