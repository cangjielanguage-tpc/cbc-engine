Пусть обработчик (поток consumer) -- это поток, который обрабатывает профильную информацию
Producer -- исполняющийся файбер.
Consumer -- отдельный поток-коллектор.
Файбер может мигрировать между OS-потоками. Необходимо, чтобы в каждый момент времени только один поток исполнял файбер, а передача файбера между потоками обеспечивала необходимую синхронизацию памяти.

## Описание структур
```
struct CallRecord {
    FunctionId caller;
    FunctionId callee;
};
```


```
struct FiberProfile {
    uint64_t writeSequence { 0 }; // only consumer

    std::atomic<uint64_t> publishedSequence { 0 };
    std::atomic<uint64_t> readSequence { 0 };

    std::array<CallRecord, CALL_BUFFER_SIZE> records;

    uint64_t droppedCalls { 0 }; // only producer

    std::atomic<bool> active { true };
};
```

writeSequence -- локальный номер ячейки для следующей записи
publishedSequence -- ячейка-граница доступная обработчику для чтения
readSequence -- ячейка-граница уже обработанных ячеек
records -- массив пар caller -> callee
active -- активен ли файбер
droppedCalls -- количество отброшенных пар (можно использовать для проверки, насколько много поток consumer не обработает пар и отталкиваясь от этого можно делать какие-то оптимизации)

Будем считать, что writeSequence, publishedSequence, readSequence не индексы ячеек, а логический номер события
Выполняется инвариант `readSequence <= publishedSequence <= writeSequence`


## Алгоритм записи

```
void RecordCall(
    FiberProfile& profile,
    FunctionId caller,
    FunctionId callee
)
{
    auto write = profile.writeSequence; \\ 1

    auto read = profile.readSequence.load(  \\ 2
        std::memory_order_acquire
    );  

    if (write - read >= CALL_BUFFER_SIZE) {  \\ 3
        ++profile.droppedCalls;
        return;
    }

    auto& record =
        profile.records[write & (CALL_BUFFER_SIZE - 1)]; \\ 4
    record.caller = caller;
    record.callee = callee;

    profile.writeSequence = write + 1;  \\ 5
 
    profile.publishedSequence.store(  \\ 6
        write + 1,
        std::memory_order_release
    );
}
```
1. Читаем номер следующего события, которое должен записать производитель

2. Читаем номер первого ещё не обработанного события. acquire не гарантирует, что будет прочитано самое последнее значение readSequence. Допускается чтение более старого значения. Производитель может ошибочно посчитать буфер заполненным, но не перезапишет необработанное событие.

3. Проверяем сколько пар уже есть на обработке `write - read`, если это значение больше, чем размер буфера, то не записываем его

4. Так как место для записи в массиве есть, то записываем пару с вычислением физического индекса ячейки

5. Увеличиваем счётчик событий

6. Публикуем номер события, которое можно читать обработчику. release обеспечивает публикацию предшествующих записей. Если коллектор читает соответствующее значение с acquire, между операциями устанавливается отношение synchronizes-with.

## Алгоритм чтения

Функция чтения должна обеспечивать следующие свойства:
1. Каждое успешно записанное событие учитывается не более одного раза.
2. События обрабатываются в порядке записи в буфер.
3. Коллектор не читает незавершённые записи.
4. Производитель не перезаписывает ячейку, пока коллектор её обрабатывает.
5. Коллектор не блокирует производителя при обработке статистики.
```
void ConsumeProfile(
    FiberProfile& profile
)
{
    auto read = profile.readSequence.load( \\  1
        std::memory_order_relaxed
    );

    auto published = profile.publishedSequence.load(  \\ 2
        std::memory_order_acquire
    );

    while (read < published) {
        const auto& record = 
            profile.records[read & (CALL_BUFFER_SIZE - 1)]; \\ 3
        CallEdge edge {
            record.caller,
            record.callee
        };

        auto count = ++counters[edge];

        if (count == HOT_CALL_COUNT) {
            LogHotCall(edge, count);
        }

        ++read;

        profile.readSequence.store(  \\ 4
            read,
            std::memory_order_release
        );
    }
}
```
1. Читаем номер первого события, которое обработчик ещё не обработал. memory_order_relaxed гарантирует, что мы получим некоторое корректное значение атомарной переменной, а не частично записанное число. C++ гарантирует, что загрузка не прочитает значение, предшествующее последней записи в эту переменную, выполненной тем же потоком до загрузки.
2. Читаем границу опубликованных событий с memory_order_acquire. Это означает, что все операции записи полей события, предшествующие публикации, происходят до последующих чтений коллектора в смысле happens-before. acquire не означает, что коллектор обязательно получит самое новое значение publishedSequence.
3. Читаем в цикле опубликованные записи
4. Публикуем readSequence с memory_order_release. 

## Проблемы
1. переполнение счётчиков
2. возможное отбрасывание свежих пар