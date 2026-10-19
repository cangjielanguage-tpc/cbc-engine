# Проверка протокола буферов с помощью CDSChecker

Модель проверяет взаимодействие одного писателя и одного читателя в
`FiberProfile`. Размер буфера уменьшен до одной записи, чтобы CDSChecker мог
полностью перебрать межпоточные исполнения. Три публикации заставляют писателя
повторно использовать первый буфер.

Обычные обращения к буферам заменены на `store_32` и `load_32` из CDSChecker.
Благодаря этому инструмент сообщает об ошибке, если читатель и писатель могут
одновременно обратиться к одной записи. `MODEL_ASSERT` дополнительно проверяет,
что опубликованная запись была инициализирована, а индексы читателя и писателя
не совпадают. Один и тот же опубликованный буфер разрешено читать повторно:
счётчики профилировщика живут только в пределах одного прохода collector.

## Подготовка CDSChecker
```bash
git clone https://github.com/computersforpeace/model-checker.git
cd model-checker
git apply /path/to/cbc-engine-initial-stage/src/profiler/model_check/cdschecker-modern-toolchain.patch
```

Patch увеличивает bootstrap allocator CDSChecker. Исходного размера 4 KiB не
хватает современному `libstdc++`. Сам CDSChecker собирается в режиме C++11,
поскольку его исходный код использует удалённые в C++17 exception
specifications.

## Запуск

Из каталога `cbc-engine-initial-stage`:

```bash
src/profiler/model_check/run.sh /path/to/model-checker
```

Корректная модель должна завершиться с таким результатом:

```text
Number of buggy executions: 0
```

Проверка ограничена safety-свойствами протокола и предполагает, что у каждого
`FiberProfile` есть только один активный писатель, читатель также один, а после
`DestroyFiberProfile` вызовы `RecordCall` больше не выполняются.
