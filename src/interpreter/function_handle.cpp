#include <mutex>
#include <unordered_map>
#include <variant>

#include "cbc/dispatcher_rt.h"
#include "cbc/rewriter.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/reader.h"
#include "function_handle.h"

namespace Interpretation {

constexpr size_t MAX_DIRECT_CALL_TRAMPOLINES_COUNT = 2048;
static DynamicFunctionHandle* directCallFuhs[MAX_DIRECT_CALL_TRAMPOLINES_COUNT];

class FunctionHandleManager::Impl {
public:
    std::mutex lock;
    std::unordered_map<Engine::Identifier<Symlevel::MethodDefinition>, TaggedFunctionHandle> fuhMap;
    size_t directCallFuhCount;
};

FunctionHandleManager::FunctionHandleManager() : impl(std::move(std::make_unique<FunctionHandleManager::Impl>())) {}

FunctionHandleManager::~FunctionHandleManager()                               = default;
FunctionHandleManager::FunctionHandleManager(FunctionHandleManager&& manager) = default;

TaggedFunctionHandle FunctionHandleManager::AcquireTagged(
    Engine::Session& session, Engine::Identifier<Symlevel::MethodDefinition> methodDef
)
{
    std::lock_guard guard(impl->lock);
    auto res = impl->fuhMap.find(methodDef);
    if (res != impl->fuhMap.end()) {
        return res->second;
    }
    auto fuh                = new DynamicFunctionHandle(nullptr, nullptr, nullptr, methodDef);
    impl->fuhMap[methodDef] = fuh;
    return fuh;
}

FunctionHandle* FunctionHandleManager::Acquire(
    Engine::Session& session, Engine::Identifier<Symlevel::MethodDefinition> methodDef
)
{
    auto fuh = AcquireTagged(session, methodDef);
    if (std::holds_alternative<DynamicFunctionHandle*>(fuh)) {
        return std::get<DynamicFunctionHandle*>(fuh);
    } else {
        return std::get<StaticFunctionHandle*>(fuh);
    }
}

ExecBytecodeInfo* FunctionHandleManager::Prepare(Engine::Session& session, DynamicFunctionHandle* fuh)
{
    auto def = Symlevel::MethodDefinition::Resolve(session, fuh->methodDef);

    std::lock_guard guard(fuh->lock);

    if (auto bytecode = fuh->bytecode.load(); bytecode != nullptr) {
        return bytecode;
    }

    auto offset = def.GetCodeOffs();
    auto code   = Symlevel::Reader::Read(session, def.FileId(), offset);

    Emitter::Emitter emitter;
    Cbc::Rewriter rewriter(nullptr, code, emitter);
    rewriter.Interpret();

    auto& heap         = session.GetEngine().CodeHeap();
    auto rewrittenCode = emitter.Build(heap);

    ExecBytecodeInfo bytecode = {
        .code = rewrittenCode,
        // TODO: initialize rest
    };

    fuh->bytecode.store(new ExecBytecodeInfo(bytecode));

    // Return via reload from `fuh->descriptor` to guarantee proper memory-model semantics:
    // fields (and fields of fields) would be visible from other threads
    // if the content of desc or desc itself would be published through "relaxed" (or race) stores
    // (explicitly in the codebase, or implictly in ASM or interpreter).
    return fuh->bytecode.load();
}

// TODO: Never inline
ExecBytecodeInfo* PrepareBytecode(DynamicFunctionHandle* fuh)
{
    Engine::Session session(Engine::GetEngineInstance());
    auto& manager = FunctionHandleManager::Of(session);
    return manager.Prepare(session, fuh);
}

static uint64_t FakeTrampoline0()
{
    DynamicFunctionHandle* fuh = directCallFuhs[0];
    auto bytecode              = fuh->bytecode.load();
    if (!bytecode) {
        bytecode = PrepareBytecode(fuh);
    }

    printf("Hello from trampoline 0\n");
    return 0;
}

static void* GetTrampoline(size_t i)
{
    ASSERTION(i == 0, "FIXME: support multiple trampolines");
    return (void*)&FakeTrampoline0;
}

void* FunctionHandleManager::GetFunctionPtr(TaggedFunctionHandle fuh)
{
    if (auto* staticFuh = std::get_if<StaticFunctionHandle*>(&fuh)) {
        return (*staticFuh)->function;
    } else {
        auto dynFuh = std::get<DynamicFunctionHandle*>(fuh);
        std::lock_guard guard(impl->lock);
        size_t i = 0;
        for (; i < impl->directCallFuhCount; i++) {
            if (directCallFuhs[i] == dynFuh) {
                return GetTrampoline(i);
            }
        }
        directCallFuhs[i] = dynFuh;
        return GetTrampoline(i);
    }
}

} // namespace Interpretation
