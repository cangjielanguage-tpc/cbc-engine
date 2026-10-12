Пусть обработчик (поток consumer) -- это поток, который обрабатывает профильную информацию
Producer -- исполняющийся файбер.
Consumer -- отдельный поток-коллектор.
Файбер может мигрировать между OS-потоками. Необходимо, чтобы в каждый момент времени только один поток исполнял файбер, а передача файбера между потоками обеспечивала необходимую синхронизацию памяти.

Основная идея алгоритма: Producer записывает события в один буфер, пока Consumer обрабатывает другой. При заполнении текущего буфера Producer пытается передать его Consumer и переключиться на свободный буфер. Если второй буфер ещё не освобождён, Producer продолжает запись в текущий, перезаписывая наиболее старые события.

## Описание структур
```
class FiberProfile {
public:
    void RecordCall(FunctionId caller, FunctionId callee);

private:
    static constexpr uint32_t CALL_BUFFER_SIZE = 4096;

    struct CallRecord {
        FunctionId caller;
        FunctionId callee;
    };

    struct CallBuffer {
        std::array<CallRecord, CALL_BUFFER_SIZE> records;
        std::atomic<bool> ready { false };
    };

    void TryPublish();

    std::array<CallBuffer, 2> buffers;

    uint32_t writerIndex { 0 };
    uint32_t cursor { 0 };
    uint32_t count { 0 };

    std::atomic<bool> active { true };
};
```

- CALL_BUFFER_SIZE — количество записей в одном буфере.
- CallRecord — одна пара вызова (caller, callee).
- records — массив пар вызовов функций.
- ready — атомарный флаг, показывающий, передан ли буфер Consumer.
- buffers — два буфера для сбора событий одного файбера.
- writerIndex — индекс буфера, в который Producer записывает события. Принимает значения 0 или 1.
- cursor — физический индекс ячейки для следующей записи в текущем буфере.
- count — количество действительных записей в текущем буфере, не превышающее CALL_BUFFER_SIZE.
- active — флаг, определяющий, завершил ли Producer работу с профилем.
Переменные writerIndex, cursor и count изменяются только Producer. Пока файбер активен, Consumer не обращается к ним.

### Состояния буферов
Каждый буфер может находиться в одном из трёх состояний:
1. WRITING — буфер принадлежит Producer, который может изменять его содержимое.
2. READY — Producer опубликовал буфер, он доступен Consumer и больше не изменяется Producer.
3. READING — Consumer обрабатывает опубликованный буфер.
Состояния READY и READING представлены одним значением ready == true. Consumer сохраняет этот флаг до завершения обработки.
Значение ready == false означает, что буфер не передан Consumer. Он может быть текущим буфером Producer или свободным буфером.
Инвариант: Producer не изменяет буфер, у которого ready == true, а Consumer не читает содержимое активного буфера Producer.

## Алгоритм записи

```
void RecordCall(FunctionId caller, FunctionId callee)
    {
        auto& current = buffers[writerIndex];
        current.records[cursor] = CallRecord { caller, callee };

        cursor = (cursor + 1) & (CALL_BUFFER_SIZE - 1);
        if (count < CALL_BUFFER_SIZE) {
            ++count;
        }

        if (cursor == 0) {
            TryPublish();
        }
    }
```
1. Получаем текущий буфер для записи.
2. Записываем пару вызовов в текущую ячейку.
3. Вычисляем индекс следующей ячейки.
4. Обновляем количество действительных записей
5. Проверяем, можно ли отдавать буфер Comsumer`у

## Алгоритм публикации буфера

```
void FiberProfile::TryPublish()
{
    const uint32_t nextIndex = writerIndex ^ 1u;
    auto& next = buffers[nextIndex];

    if (next.ready.load(std::memory_order_acquire)) {
        return;
    }

    auto& current = buffers[writerIndex];

    current.ready.store(
        true,
        std::memory_order_release
    );

    writerIndex = nextIndex;
    cursor = 0;
    count = 0;
}
```

1. Вычисляем индекс второго буфера
2. Получаем ссылку на второй буфер
3. Проверяем, освободил ли Consumer второй буфер
Если `ready == true`, второй буфер принадлежит Consumer. Producer не может начать запись в него. Завершаем `TryPublish()`
4. Получаем текущий буфер
5. Публикуем текущий буфер.
6. Переключаем Producer на второй буфер

## Алгоритм чтения

Функции Consumer должны обеспечивать следующие свойства:
1. Каждое опубликованное событие учитывается не более одного раза.
2. События внутри одного буфера обрабатываются в порядке записи.
3. Consumer не читает незавершённые записи.
4. Producer не перезаписывает буфер, пока Consumer его обрабатывает.
5. Consumer не блокирует Producer при обработке статистики.

```
void ConsumePublished(FiberProfile& profile)
{
    for (auto& buffer : profile.buffers) {
        if (!buffer.ready.load(std::memory_order_acquire)) {
            continue;
        }

        ProcessBuffer(buffer, 0, FiberProfile::CALL_BUFFER_SIZE);

        buffer.ready.store(
            false,
            std::memory_order_release
        );
    }
}
```
1. Перебираем оба буфера файбера.
2. Проверяем, опубликован ли буфер.
Если `ready == false`, Consumer пропускает буфер.
Если `ready == true`, Consumer получает право читать его содержимое.
3. Обрабатываем опубликованный буфер
4. Освобождаем обработанный буфер. Consumer устанавливает `ready = false`

### Обработка одного буфера
```
void ProcessBuffer(
    const FiberProfile::CallBuffer& buffer,
    uint32_t start,
    uint32_t count
)
{
    for (uint32_t i = 0; i < count; ++i) {
        const auto index =
            (start + i) & (FiberProfile::CALL_BUFFER_SIZE - 1);

        ProcessRecord(buffer.records[index]);
    }
}

void ProcessRecord(const FiberProfile::CallRecord& record)
{
    CallEdge edge { record.caller, record.callee };

    const auto count = ++counters[edge];

    if (count == HOT_CALL_COUNT) {
        LogHotCall(edge, count);
    }
}
```

1. Последовательно перебираем действительные записи.
2. Вычисляем физический индекс записи.
3. Обновляем статистику счетчиков.


## Алгоритм завершения файбера

```
void DestroyFiberProfile(FiberProfileHandle& profile)
{
    if (profile) {
        profile->active.store(
            false,
            std::memory_order_release
        );

        profile.reset();
    }
}
```
## Проблемы
1. переполнение счётчиков
2. возможное отбрасывание свежих пар