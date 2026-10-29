## Обозначения

Producer — исполняющийся файбер, который записывает вызовы функций.
Consumer — отдельный поток-коллектор, который периодически обрабатывает профильную информацию.

Файбер может мигрировать между OS-потоками. Алгоритм предполагает, что в каждый момент времени один `FiberProfile` использует только один Producer, а передача файбера между OS-потоками обеспечивает необходимую синхронизацию памяти.
Consumer существует в одном экземпляре.

## Основная идея

У каждого файбера есть два буфера. В каждый момент один из них является текущим writer-буфером Producer, а второй может быть доступен Consumer. Если Consumer ничего не читает, после заполнения текущего буфера Producer переключается на следующий, чтобы поддерживать актуальность профильной информации. Поэтому при обычной работе Producer попеременно заполняет буферы `0` и `1`, а Consumer читает буфер, противоположный текущему writer-буферу.

При заполнении текущего буфера Producer временно переводит состояние читателя в `TRANSITION`:

- если Consumer ничего не читает (`INACTIVE`), Producer переключается на второй буфер, а заполненный буфер становится доступен Consumer;
- если Consumer читает (`ACTIVE`), Producer временно не переключается на занятый Consumer буфер и циклически перезаписывает текущий writer-буфер. После завершения чтения переключение снова становится возможным при следующем заполнении.

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
        INACTIVE,
        ACTIVE,
        TRANSITION
    };

    std::array<CallBuffer, 2> buffers;

    uint32_t cursor { 0 };
    uint32_t count { 0 };

    std::atomic<uint32_t> sharedWriterIndex { NO_PUBLISHED_BUFFER };
    std::atomic<ReaderState> readerState { ReaderState::INACTIVE };
    std::atomic<bool> active { true };
};
```

- `CallRecord` содержит идентификатор вызывающей функции и абсолютный адрес инструкции вызова в её байткоде.
- `buffers` — два массива записей одного файбера.
- `cursor` — позиция следующей записи в текущем буфере.
- `count` — количество действительных записей в текущем буфере, не больше `CALL_BUFFER_SIZE`.
- `sharedWriterIndex` — атомарно опубликованный индекс текущего writer-буфера. Producer получает свой индекс как `sharedWriterIndex & 1`, а Consumer после первой публикации читает буфер с противоположным индексом.
- `NO_PUBLISHED_BUFFER == 2` — начальное значение `sharedWriterIndex`. Для Consumer оно означает, что заполненный буфер ещё не опубликован; для Producer выражение `2 & 1` выбирает начальный writer-буфер `0`.
- `readerState` координирует снимок Consumer и переключение Producer.
- `active` показывает, может ли Producer ещё обращаться к профилю.

`cursor` и `count` изменяет только Producer. Пока профиль активен, Consumer не читает эти поля. Прямое чтение producer-local полей разрешено лишь после завершения файбера.

## Состояния читателя

`readerState` принимает три значения:

1. `INACTIVE` — Consumer не читает буфер. Producer может переключить writer-буфер.
2. `ACTIVE` — Consumer зафиксировал снимок и читает противоположный буфер. Producer продолжает писать в текущий буфер.
3. `TRANSITION` — Producer атомарно решает, можно ли переключить буфер. Пока активно это состояние, Consumer не может начать или завершить свой переход.

Основной инвариант:

```text
readerIndex != writerIndex
```

Пока Consumer находится в состоянии `ACTIVE`, значение writer-индекса не может измениться. Поэтому Producer и Consumer не обращаются к одной записи буфера одновременно.

## Запись события

```cpp
void RecordCall(FunctionId caller, CallSiteId callSite)
{
    const auto writerIndex =
        sharedWriterIndex.load(std::memory_order_relaxed) & 1U;
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

1. relaxed-загрузка индекса writer-буфера;
2. запись пары `(caller, callSite)`;
3. обновление циклического индекса;
4. обновление локального счётчика записей.

`TryPublish` вызывается только после полного прохода буфера.

Если переключиться не получилось, `cursor` уже равен нулю, `count` остаётся равным размеру буфера, и Producer начинает перезаписывать самые старые записи текущего буфера.

## Публикация и смена буфера

```cpp
void FiberProfile::TryPublish()
{
    auto previous = readerState.load();
    while (previous == ReaderState::TRANSITION ||
           !readerState.compare_exchange_weak(previous, ReaderState::TRANSITION)) {
        if (previous == ReaderState::TRANSITION) {
            std::this_thread::yield();
            previous = readerState.load();
        }
    }

    if (previous == ReaderState::INACTIVE) {
        const auto writerIndex =
            (sharedWriterIndex.load(std::memory_order_relaxed) & 1U) ^ 1U;
        cursor = 0;
        count = 0;
        sharedWriterIndex.store(writerIndex);
    }

    readerState.store(previous);
}
```

Producer выполняет следующие действия:

1. С помощью CAS заменяет `INACTIVE` или `ACTIVE` на `TRANSITION` и запоминает предыдущее значение.
2. Если предыдущее состояние было `INACTIVE`, вычисляет противоположный writer-индекс и публикует его через `sharedWriterIndex`.
3. Если предыдущее состояние было `ACTIVE`, не переключает буфер: противоположный буфер в этот момент принадлежит Consumer.
4. Восстанавливает предыдущее состояние `INACTIVE` или `ACTIVE`.

`sharedWriterIndex` содержит индекс нового writer-буфера. Буфер Consumer вычисляется как `sharedWriterIndex ^ 1`.

## Чтение опубликованного буфера

### Захват и освобождение права чтения

Consumer не может просто записать в `readerState` значение `ACTIVE` или `INACTIVE`: между загрузкой состояния и записью Producer мог перевести его в `TRANSITION`. Обычный `store` тогда затёр бы состояние Producer и позволил бы одновременно
переключить и читать один буфер.

Поэтому Consumer меняет состояние только условно, с помощью `ChangeReaderState`. Функция выполняет переход `from -> to`, только если в момент CAS атомарная переменная всё ещё равна `from`:

```cpp
void ChangeReaderState(
    FiberProfile& profile,
    ReaderState from,
    ReaderState to
)
{
    while (true) {
        auto expected = from;
        if (profile.readerState.compare_exchange_weak(expected, to)) {
            return;
        }
        std::this_thread::yield();
    }
}
```

Функция используется для двух разных действий:

- `INACTIVE -> ACTIVE` — Consumer получает право читать опубликованный буфер;
- `ACTIVE -> INACTIVE` — Consumer сообщает, что чтение закончено и буфер снова можно использовать при следующем переключении Producer.

1. `expected` инициализируется значением `from`.
2. `compare_exchange_weak` атомарно сравнивает `readerState` с `expected`.
3. Если они равны, CAS записывает `to`. Consumer получил требуемое состояние, и функция завершается.
4. Если они не равны или произошёл допустимый для weak CAS spurious failure, операция завершается неудачей. Consumer вызывает `yield` и повторяет попытку.

`std::this_thread::yield()` позволяет планировщику запустить Producer, который должен закончить короткий участок `TRANSITION` и восстановить `INACTIVE` либо `ACTIVE`.



Если при начале чтения Producer уже находится в `TRANSITION`, переход `INACTIVE -> ACTIVE` не срабатывает. Consumer ждёт, пока Producer закончит публикацию и восстановит `INACTIVE`. Аналогично, если Producer успел выполнить `ACTIVE -> TRANSITION` перед окончанием чтения, переход Consumer `ACTIVE -> INACTIVE` ждёт восстановления `ACTIVE`. Это не позволяет Consumer ни войти в секцию чтения, ни выйти из неё посередине смены буфера.

### Выбор и обработка буфера

После успешного перехода `INACTIVE -> ACTIVE` чтение выполняется следующим образом:

```cpp
void ConsumePublished(FiberProfile& profile, CallCounters& counters)
{
    ChangeReaderState(
        profile,
        ReaderState::INACTIVE,
        ReaderState::ACTIVE
    );

    const auto writerIndex = profile.sharedWriterIndex.load();

    if (writerIndex != FiberProfile::NO_PUBLISHED_BUFFER) {
        const auto readerIndex = writerIndex ^ 1U;
        ProcessBuffer(profile.buffers[readerIndex], 0,
                      FiberProfile::CALL_BUFFER_SIZE, counters);
    }

    ChangeReaderState(
        profile,
        ReaderState::ACTIVE,
        ReaderState::INACTIVE
    );
}
```

1. Consumer выполняет переход `INACTIVE -> ACTIVE`. Если Producer находится в `TRANSITION`, Consumer ждёт.
2. Загружает `sharedWriterIndex`.
3. До первой публикации видит `NO_PUBLISHED_BUFFER` и ничего не читает.
4. После публикации читает буфер `sharedWriterIndex ^ 1`.
5. После обработки выполняет переход `ACTIVE -> INACTIVE`. Если Producer временно заменил `ACTIVE` на `TRANSITION`, Consumer ждёт, пока тот восстановит `ACTIVE`.

Один и тот же буфер может обрабатываться в нескольких последовательных проходах. Это допустимо, потому что статистика предыдущего прохода не сохраняется.

## Sequential consistency и корректность

Заполнение буфера происходит до:

```cpp
sharedWriterIndex.store(writerIndex);
```

Записи заполненного буфера happens-before их чтения Consumer.

Consumer завершает чтение до перехода `ACTIVE -> INACTIVE`. Следующий успешный CAS Producer, увидевший `INACTIVE`, выполняется после этого перехода. Поэтому Producer не начинает повторное использование освобождённого буфера до завершения чтения.

Разбор гонок при захвате состояния:

- если Consumer первым выполнил `INACTIVE -> ACTIVE`, Producer увидит предыдущее состояние `ACTIVE` и не переключит буфер;
- если Producer первым выполнил `INACTIVE -> TRANSITION`, Consumer не сможет начать чтение, пока Producer не переключит буфер и не восстановит `INACTIVE`;
- если Producer временно выполнил `ACTIVE -> TRANSITION`, Consumer не сможет завершить переход `ACTIVE -> INACTIVE`, пока Producer не восстановит `ACTIVE`.

Алгоритм не использует мьютекс на пути записи события. Однако это не строгая lock-free гарантия прогресса: поток, остановленный внутри состояния `TRANSITION`, может временно задержать Consumer.

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
        profile->active.store(false);
        profile.reset();
    }
}
```

Consumer загружает `active` операцией `seq_cst`. Если Producer завершён, он больше не изменяет producer-local поля и текущий буфер. Consumer сначала обрабатывает опубликованный полный буфер, затем вычисляет текущий writer-индекс как `sharedWriterIndex & 1` и безопасно читает оставшиеся `count` записей текущего, возможно частично заполненного буфера. После этого профиль удаляется из списка активных.

## Верификация с помощью CDSChecker

Протокол состояний и буферов проверялся с помощью CDSChecker — model checker для атомарных операций и memory model C11/C++11.

### Устройство модели

Модель сохраняет существенные части алгоритма:

- два буфера;
- одного Producer и одного Consumer;
- состояния `INACTIVE`, `ACTIVE`, `TRANSITION`;
- те же CAS-переходы, операции `seq_cst` и producer-only relaxed-загрузки writer-индекса;
- `sharedWriterIndex` со значением `NO_PUBLISHED_BUFFER` до первой публикации;
- переключение только при предыдущем состоянии `INACTIVE`.

Чтобы перебрать все состояния, размер буфера уменьшён до одного. Producer выполняет три заполнения: этого достаточно, чтобы повторно использовать первый буфер и обнаружить возможное одновременное чтение и запись. Потоки создаются через C11 `thrd_create`.

Схема модели выглядит так:

```cpp
void Producer(void*)
{
    for (uint32_t batch = 1; batch <= 3; ++batch) {
        for (uint32_t i = 0; i < BUFFER_SIZE; ++i) {
            const auto writerIndex = sharedWriterIndex.load(std::memory_order_relaxed) & 1U;
            store_32(&buffers[writerIndex][cursor], batch);
            cursor = (cursor + 1U) & (BUFFER_SIZE - 1U);
        }
        TryPublish();
    }
}

void Consumer(void*)
{
    // CAS: INACTIVE -> ACTIVE
    int expected = INACTIVE;
    while (!readerState.compare_exchange_weak(expected, ACTIVE)) {
        expected = INACTIVE;
        thrd_yield();
    }

    const auto currentSharedWriterIndex = sharedWriterIndex.load();
    if (currentSharedWriterIndex != NO_PUBLISHED_BUFFER) {
        const auto readerIndex = currentSharedWriterIndex ^ 1U;
        MODEL_ASSERT(readerIndex != currentSharedWriterIndex);

        const auto currentWriterIndex = static_cast<uint32_t>(sharedWriterIndex.load());
        MODEL_ASSERT(readerIndex != currentWriterIndex);

        const auto record = load_32(&buffers[readerIndex][0]);

        MODEL_ASSERT(record != 0);
    }

    // CAS: ACTIVE -> INACTIVE
    expected = ACTIVE;
    while (!readerState.compare_exchange_weak(expected, INACTIVE)) {
        expected = ACTIVE;
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
const auto currentWriterIndex = sharedWriterIndex.load(...);
MODEL_ASSERT(readerIndex != currentWriterIndex);
```

Проверяет, после возможного переключения планировщика, что Producer не смог сменить индекс на буфер Consumer во время чтения.

```cpp
MODEL_ASSERT(record != 0);
```

Проверяет, что Consumer не прочитал буфер до его первой полной инициализации.

Также проверяется отсутствие deadlock в конечной модели. При намеренном нарушении протокола — если разрешить Producer переключать буфер при предыдущем состоянии `ACTIVE` — CDSChecker обнаруживает data race между `load_32` Consumer и `store_32` Producer. Это использовалось как отрицательная проверка, что модель действительно чувствительна к нарушению основного инварианта.

Для корректной версии итог model checking содержит:

```text
Number of complete, bug-free executions: 466
Number of redundant executions: 150
Number of buggy executions: 0
Number of infeasible executions: 0
Total executions: 616
```

### Границы проверки

Модель проверяет safety-свойства уменьшенного протокола при следующих предположениях:

1. Для одного `FiberProfile` одновременно существует только один Producer.
2. Consumer существует в одном экземпляре.
3. После `DestroyFiberProfile` больше не вызывается `RecordCall`.
4. Миграция файбера между OS-потоками сама корректно передаёт владение и видимость producer-local данных.
