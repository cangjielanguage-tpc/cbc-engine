# Этапы реализации динамического инлайнинга CBC → CBC

## Этап 0. Логирование

### Цель

Получить подробные логи при нахождении горячего вызова функции.

### Что сделать

1. Расширить вывод profiler-а данными:
   - `callerFuh` и `calleeFuh`;
   - исходный CBC-offset и runtime-offset вызова;
   - число вызовов и признак hot;
   - размер caller/callee bytecode;
   - version и причина отказа.
2. Расширить вывод `ExecBytecodeInfo`:
   - version и адрес самой `ExecBytecodeInfo`;
   - saved I/F register masks;
   - ABI и stack-pointer metadata;
   - exception table и origin ranges;

### Основные файлы

- `src/profiler/profiler.cpp`;
- `src/profiler/profiler_logger.{h,cpp}`;
- `src/interpreter/code.{h,cpp}`;

### Результат этапа

Поведение программы не меняется. В логах достаточно информации, чтобы однозначно найти горячую инструкцию и сравнить версии кода.

## Этап 1. Разбор call site

### Цель

Убрать зависимость profiler record от предположения, что `callerFuh->bytecode` всё ещё указывает на тот code buffer, из которого пришёл абсолютный адрес вызова.

### Что сделать

1. В collector-е сначала агрегировать записи по текущему ключу `(callerFuh, absoluteCallSiteAddress)`, а проверку выполнять один раз для каждого уникального call edge:
   - загрузить текущий `ExecBytecodeInfo*` caller-а;
   - взять начало `codeBegin = current->code.bytecode` и вычислить конец `codeEnd = codeBegin + current->code.bytecodeSize`;
   - проверить условие `codeBegin <= absoluteCallSiteAddress < codeEnd`;
   - адрес вне диапазона сразу отбросить как stale, не декодируя старый buffer;
   - для текущего диапазона вычислить `runtimeOffset = absoluteCallSiteAddress - codeBegin`;
   - проверить opcode, декодировать literal и перевести runtime-offset в исходный CBC-offset.
2. Добавить `InlineRequest` с `(callerFuh, expectedVersion, original CBC offset)`. Поток, выполняющий rewrite, повторно проверяет `callerFuh->bytecode == expectedVersion`.
3. Старые code buffers пока сохранять до shutdown: collector мог получить snapshot старой current version непосредственно перед публикацией.
4. При публикации новой версии считать все requests с другим `expectedVersion` неактуальными для новых inline decisions.
5. Зафиксировать инвариант copy-and-publish: каждая версия получает новый неперекрывающийся code allocation, старый buffer не изменяется и не освобождается. Перед CAS проверять непересечение диапазонов старой и новой versions. Без этого инварианта проверка адреса по current range некорректна.
6. Общий флаг публикации использовать только как уведомление collector-а или границу profiling epoch. Старый активный frame может продолжать писать старые адреса неограниченно долго.
7. Опционально добавить в каждый `FiberProfile` атомарный `resetRequested`. После успешного inline rewrite выставлять его фоновым потоком, а обрабатывать только в `TryPublish`:
   - под `bufferMutex` сбросить `cursor` и `count`;
   - установить `sharedWriterIndex = NO_PUBLISHED_BUFFER`;
   - не публиковать заполненный старый buffer;
   - оставить `recordCount` монотонным;
   - обработать отдельно завершение fiber-а до следующего `TryPublish`.

   Эта очистка не добавляет операций в обычный `RecordCall`, но не отменяет фильтрацию versions и повторную проверку version перед rewrite.

### Основные файлы

- `src/profiler/fiber_profile.h`;
- `src/profiler/profiler.cpp`;
- `src/interpreter/interpretation_loop.cpp`.

### Результат этапа

Producer выполняет те же операции и записывает те же два машинных слова, что и до поддержки инлайнинга. Collector способен различать records разных code buffers одного caller-а и учитывать для оптимизации только текущую version.

## Этап 2. Версии bytecode и безопасная публикация

### Цель

Создать и атомарно опубликовать новую версию `ExecBytecodeInfo`, идентичную старой по поведению.

### Что сделать

1. Добавить `version` к исполняемой версии. Начальная версия имеет version 0, каждая успешно опубликованная — предыдущее значение + 1.
2. Сделать ownership версий явным:
   - текущая версия доступна через `DynamicFunctionHandle`;
   - старые версии помещаются в список retired;
   - на первых этапах retired versions не освобождаются до shutdown.
3. Реализовать пробную операцию `RebuildAndPublish`, которая заново вызывает rewrite исходного метода без инлайнинга.
4. Перед CAS проверять, что текущая версия совпадает с версией, на основе которой строился результат. При конфликте результат не публиковать.

### Основные файлы

- `src/interpreter/code.h`;
- `src/interpreter/function_handle.{h,cpp}`;
- `src/cbc/isa_rewriter.{h,cpp}`;

### Результат этапа

Новые входы в функцию могут выбрать заново собранную версию. Семантика bytecode пока не меняется.

## Этап 3. Добавление версии в interpreter frame

### Цель

Гарантировать, что активный frame использует code, frame layout и metadata одной версии, даже если другой поток публикует новую версию того же `DynamicFunctionHandle`.

### Что сделать

1. Добавить в layout interpreter frame слот `ExecBytecodeInfo* pinnedVersion`.
2. На входе в функцию один раз загрузить текущую версию и записать её в слот.
3. Перевести на pinned version:
   - создание `ByteReader`;
   - загрузку literal table;
   - выделение и освобождение frame по `frameSize`;
   - PUSH/POP saved registers;
   - GC frame scanning;
   - stack expansion и stack movement;
   - exception handling и stack trace;
   - profiler records.
4. Обновить frame offsets и static assertions.
5. Внести симметричные изменения в x86_64 и AArch64 trampolines.
6. Проверить memory ordering assembly-входа.

### Основные файлы

- `src/arch_os/x86_64_linux/trampolines.S`;
- `src/arch_os/aarch64_linux/trampolines.S`;
- `src/arch_os/*/asm_export*` и общие frame constants;
- `src/runtimesupport/impl/gc_support.cpp`;
- `src/runtimesupport/impl/stack_expansion.cpp`;
- `src/runtimesupport/impl/exception_handling.cpp`.

### Результат этапа

Поток A может продолжать version 0, пока поток B публикует version 1. Все runtime helpers потока A используют metadata version 0.

## Этап 4. Обработка hot call и без изменения кода

### Цель

Формализовать решение об инлайнинге и определить место выполнения rewrite (в потоке читателя или в отдельном).

### Что сделать

1. Поддержать один из вариантов выполнения:
   - **в потоке collector-а:** после завершения обхода всех profiles, освобождения `profilesMutex`/`bufferMutex` и формирования списка кандидатов collector последовательно строит запросы на инлайнинг, выполняет rewrite и публикует версию;
   - **в отдельном worker-е:** collector помещает immutable request в очередь, а worker выполняет те же действия асинхронно.

2. Сформировать request:

   ```text
   callerFuh
   caller version
   original CBC call-site offset
   expected calleeFuh
   profile count
   ```
3. Поток, который непосредственно выполняет rewrite повторно проверяет:
   - version не изменилась;
   - по offset всё ещё находится прямой вызов;
   - resolver возвращает тот же dynamic callee;
   - caller и callee являются CBC-функциями.
4. Выполнить обычный rewrite без встраивания.
5. Логировать причину каждого отказа: stale version, recursion, size limit, metadata limitation и другие.

### Основные файлы

- `src/profiler/profiler.cpp` для первоначального синхронного варианта;
- `src/interpreter/function_handle.{h,cpp}`;

### Результат этапа

Полный управляющий путь от hot call до новой version работает, но код вызова ещё сохраняется.

## Этап 5. Минимальный инлайнер

### Цель

Заменить один `DIRECT_CALL_2I` телом callee.

### Ограничения первой версии

Разрешать callee только если он:

- не имеет typed/untyped stack slots;
- не содержит вложенных вызовов;
- не содержит state points, GC operations и потенциально бросающих операций;
- не имеет exception regions;
- не использует SRET, generic, tail или stack arguments;
- имеет ограниченный размер;
- использует регистры по заранее проверяемой безопасной схеме.

### Что сделать

1. Выделить `RewriteContext` для caller и callee:
   - `methodDef`;
   - `MethodCode`;
   - `Resolver`;
   - namespace labels;
   - return continuation.
2. Изменить ключ labels с одного offset на `(inlineInstanceId, CBC offset)`.
3. В `IsaRewriter::CallDirect` проверять нужно ли инлайнить:
   - emit-ить обычный call;
   - или переключать context и парсить callee.
4. Каждый callee `RET` заменить на сохранение результата и jump к общему `inline_exit`.
5. После `inline_exit` поместить результат в регистр назначения исходного call.
6. Сохранить origin caller/callee хотя бы в диагностической таблице.
7. Перед публикацией проверить размер literals и representability branches.

### Основные файлы

- `src/cbc/isa_rewriter.{h,cpp}`;
- `src/cbc/emitter/*`;
- `src/cbc/offsets_index.*`.

### Результат этапа

Для маленькой функции новая версия caller-а не содержит выбранный `DIRECT_CALL_2I`; вычисляемый результат совпадает с обычным вызовом.

## Этап 6. Регистры и объединённый frame

### Цель

Убрать ограничения на stack slots и обеспечить сохранение всех значений caller-а, живых через call site.

На этом этапе решается **семантика вычислений и layout frame**, но ещё не корректность остановки в state point. Встроенный callee может использовать регистры, primitive/reference values и собственные stack slots, однако его inline body пока не должен содержать операцию, способную запустить GC или потребовать GC metadata: allocation, `GC_POINT`, вызов runtime/другой функции и аналогичный safepoint.

### Что сделать

1. Реализовать анализ use/def и liveness исходного CBC.
2. Для call site определить значения caller-а, живые после вызова.
3. Для callee определить registers, которые могут быть перезаписаны.
4. В первой корректной реализации spill-ить пересечение в дополнительные slots:
   - до inline body сохранить значения;
   - отдельно сохранить return value;
   - на exit восстановить caller values;
   - затем записать return value в исходный destination.
5. Построить объединённый `FrameLayout`:
   - slots caller-а сохраняют адреса;
   - slots каждого inline instance получают отдельный диапазон;
   - spill slots получают отдельный диапазон;
   - соблюдается alignment.
6. Перевести все slot-based операции callee через mapping его `RewriteContext`.
7. Объединить маски используемых non-volatile registers. Prologue caller-а должен сохранять регистры, добавленные встроенным телом.
8. После корректного spill-варианта добавить register renaming и удаление лишних spill/reload.

### Основные файлы

- `src/cbc/isa_rewriter.cpp`;
- `src/cbc/frame.h`;
- новый модуль анализа CBC, если его неудобно размещать в rewriter-е;
- `src/interpreter/code.h`.

### Результат этапа

Можно встраивать функции со своими stack slots и с использованием регистров, пересекающихся с живыми регистрами caller-а.

Ссылочные значения уже можно перемещать между registers/slots как обычные значения при условии, что внутри inline body GC не запускается. GC пока не обязан знать о новом расположении этих ссылок.

## Этап 7. GC и stack-pointer metadata

### Цель

Разрешить state points и аллокации внутри встроенного callee, не теряя roots caller-а.

### Что сделать

1. Расширить представление state point:

   ```text
   rewritten label
   origin method/inline instance
   origin CBC offset
   caller-across-call state
   ```

2. Переводить callee metadata через register/slot mapping.
3. В каждой точке callee добавлять roots caller-а, живые через исходный call.
4. Reference spills отмечать как reference slots.
5. Перенести и скомпоновать:
   - `regMask`;
   - `refSlotNums`;
   - typed reference offsets;
   - `mutPairs`;
   - `StackPtrsInfo`.
6. Проверить GC metadata до, внутри и после inline body.
7. Проверить перемещение fiber stack, когда frame находится внутри встроенного callee.

### Основные файлы

- `src/cbc/isa_rewriter.cpp`;
- `src/interpreter/code.h`;
- `src/runtimesupport/impl/gc_support.cpp`;
- `src/runtimesupport/impl/stack_expansion.cpp`.

### Результат этапа

Встроенный callee может аллоцировать объекты, вызывать GC и содержать pointers into stack. GC видит одновременно состояние callee и сохранённые значения caller-а.

## Этап 8. Исключения и origin mapping

### Цель

Сохранить семантику `try/catch` через логическую границу встроенного callee и обеспечить корректную диагностику позиций.

### Что сделать

1. Добавить `RuntimeExceptionTable` в исполняемую версию:

   ```text
   rewritten range -> rewritten handler target
   origin + inline depth
   ```

2. При rewrite заранее перевести exception regions caller-а и callee в runtime ranges.
3. Сделать ranges caller-а охватывающими вставленное тело, если исходный call находился внутри `try`.
4. Обеспечить приоритет внутреннего handler-а callee перед внешним handler-ом caller-а.
5. Перевести target callee в label конкретного inline instance.
6. Перевести `engine_get_exception_handler` на pinned version и runtime table.
7. Ввести `InlineOriginTable`:

   ```text
   rewritten range -> inlineFrameId + original CBC offset

   inlineFrameId -> {
       methodDef,
       parentInlineFrameId,
       caller CBC call-site offset,
       inline depth
   }
   ```

8. Сохранить логическую цепочку inline frames. Физического interpreter frame callee после инлайнинга нет, но metadata должна представлять его так, как если бы вызов сохранился:
   - текущая инструкция относится к исходному callee;
   - parent указывает на логического caller-а и исходный call site;
   - при нескольких уровнях цепочка восстанавливается до физического метода.
9. При поиске exception handler идти по логической цепочке:
   - сначала regions текущего callee;
   - затем regions parent caller-а в позиции исходного call site;
   - при отсутствии handler-а выполнить обычный unwind физического frame caller-а.
10. Использовать origin/inline-frame table в profiler, disassembly, stack trace и source-line mapping.
11. Определить пользовательское отображение inline frames в stack trace: показывать callee и цепочку callers как синтетические логические frames либо явно маркировать их как inlined.

### Основные файлы

- `src/interpreter/code.h`;
- `src/cbc/isa_rewriter.cpp`;
- `src/runtimesupport/impl/exception_handling.cpp`;
- `src/profiler/profiler.cpp`;
- `src/cbc/offsets_index.*`.

### Результат этапа

Исключение может быть поймано внутри callee либо выйти из него в handler caller-а. Runtime position однозначно связывается с логическим исходным методом.

В call stack не сохраняется настоящий frame заинлайненной функции — это уничтожило бы основную часть выигрыша. Вместо него `InlineOriginTable` хранит достаточно информации, чтобы обработка исключений и stack trace восстановили **логический call stack**, эквивалентный незаинлайненному вызову.

## Этап 9. Несколько call sites, рекурсия и политика

### Цель

Перейти от демонстрации одного site к управляемому production-поведению без неограниченного роста кода.

### Что сделать

1. Разрешить несколько `InlinePlan` при одном rebuild caller-а.
2. Перед emission сортировать и валидировать планы по исходным CBC-offsets.
3. Ввести ограничения:
   - максимальный размер callee;
   - максимальный прирост caller-а;
   - максимальная inline depth;
   - максимальное число sites одной version;
   - лимит literal table;
   - cooldown между rewrite одного caller-а.
4. Обнаруживать прямую и косвенную рекурсию.
5. Для рекурсивного случая сначала разрешить не более одного явно ограниченного уровня.
6. При новом профиле строить следующую версию из исходного CBC и накопленного набора планов, а не пытаться повторно парсить уже rewritten runtime bytecode.
7. Определить, когда ранее встроенный site нужно сохранить, удалить или перестроить.
8. Добавить метрики code growth, compile time и числа успешных/отклонённых sites.

### Основные файлы

- модуль inliner policy/обработки; отдельный worker опционален;
- `src/cbc/isa_rewriter.cpp`;
- profiler logging.

### Результат этапа

Инлайнер способен постепенно оптимизировать несколько горячих вызовов, сохраняя контролируемый размер кода и не зацикливая rewrite.


## Этап 10. OSR (On-Stack Replacement) для долгоживущих callers

### Цель

OSR (On-Stack Replacement) — замена кода для функции, frame которой уже находится на стеке. Механизм переводит уже исполняющийся frame со старой version на новую в безопасной точке, не заставляя функцию сначала выполнить `return`, а затем войти снова.

Без OSR долго работающий цикл в `main` продолжит старый `DIRECT_CALL_2I`, даже если оптимизированная версия `main` уже опубликована.

### Подэтап 10.1. OSR polls

1. Выбрать безопасные точки перехода:
   - существующий `GC_POINT`/safepoint;
   - poll на back edge горячего цикла.
2. В poll проверять, появилась ли более новая version для текущего `fuh`.
3. Не выполнять переход в середине basic block или инструкции.

### Подэтап 10.2. Переход при совместимом frame layout

1. Строить карту:

   ```text
   old version + old rewritten offset
       -> new version + new rewritten offset
   ```

2. Сначала разрешать переход только если совпадают:
   - `frameSize`;
   - saved-register masks;
   - расположение существующих slots;
   - представление live registers.
3. В одной контролируемой операции заменить:
   - pinned version;
   - reader start/end/cursor;
   - literals.
4. Продолжить с эквивалентной позиции, обычно с начала следующей итерации.

### Подэтап 10.3. Полный state transform

1. Сохранять описание live values в OSR-точке.
2. Переносить registers и slots из старого layout в новый.
3. Поддержать изменение frame size с учётом movable fiber stack.
4. Определить metadata переходного состояния, чтобы GC не увидел наполовину преобразованный frame.
5. Сделать операцию либо неделимой относительно safepoint, либо представить transition frame отдельным валидным descriptor-ом.

### Основные файлы

- `src/interpreter/interpretation_loop.cpp`;
- assembly trampolines и frame layout;
- `src/runtimesupport/impl/gc_support.cpp`;
- `src/runtimesupport/impl/stack_expansion.cpp`;
- inliner/version manager;
- origin/OSR mapping в `ExecBytecodeInfo`.

### Результат этапа

Один и тот же физический frame долгоживущего `main` начинает следующую итерацию на новой version и перестаёт выполнять прежний call site.

## Этап 11. Оптимизация и освобождение старых версий

### Цель

Снизить overhead сгенерированного inline-кода и ограничить расход памяти после доказательства корректности.

### Что сделать

1. Оптимизировать inline body:
   - register coalescing;
   - удаление лишних spill/reload;
   - constant propagation;
   - удаление недостижимых ветвей;
   - dead-code elimination.
2. Измерять выигрыш отдельно от стоимости profiler-а и выполнения rewrite в выбранном потоке.
3. Ввести reclamation retired versions:
   - refcount frame/profiler users; либо
   - epoch/RCU с quiescent states fibers и collector-а.
4. Учитывать версии, на которые ещё ссылаются:
   - активные frames;
   - profiler buffers;
   - queued inline requests;
   - OSR mappings/transition.
5. Ограничить суммарный code cache и определить политику отказа от новых оптимизаций при достижении лимита.

### Основные файлы

- CBC analysis/rewriter;
- version manager;
- profiler buffers;
- Engine/CodeHeap ownership.

### Результат этапа

Динамический инлайнер даёт измеримый выигрыш на целевых сценариях и не накапливает старые версии без ограничений.
