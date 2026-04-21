#include <iostream>
#include <mutex>
#include <unordered_map>
#include <variant>

#include "adapters.h"
#include "cbc/isa_disasm.h"
#include "cbc/isa_rewriter.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/reader.h"
#include "function_handle.h"

namespace Interpretation {

static_assert(offsetof(DynamicFunctionHandle, c2call) == FUNCTION_HANDLE_C2CALL_OFFSET);
static_assert(offsetof(DynamicFunctionHandle, bytecode) == FUNCTION_HANDLE_BYTECODE_OFFSET);

class FunctionHandleManager::Impl {
public:
    std::mutex lock;
    std::unordered_map<Engine::Identifier<Symlevel::MethodDefinition>, TaggedFunctionHandle> fuhMap;
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
    auto i2Call             = PrepareI2Call(session, methodDef);
    auto c2Call             = PrepareC2Call(session, methodDef);
    auto fuh                = new DynamicFunctionHandle(i2Call, c2Call, methodDef);
    impl->fuhMap[methodDef] = fuh;
    return fuh;
}

FunctionHandle* FunctionHandleManager::Acquire(
    Engine::Session& session, Engine::Identifier<Symlevel::MethodDefinition> methodDef
)
{
    auto fuh = AcquireTagged(session, methodDef);
    if (std::holds_alternative<DynamicFunctionHandle*>(fuh)) {
        return &std::get<DynamicFunctionHandle*>(fuh)->base;
    } else {
        return &std::get<StaticFunctionHandle*>(fuh)->base;
    }
}

ExecBytecodeInfo* FunctionHandleManager::Prepare(Engine::Session& session, DynamicFunctionHandle* fuh)
{
    auto def = Symlevel::MethodDefinition::Resolve(session, fuh->methodDef);

    std::lock_guard guard(fuh->lock);

    if (auto bytecode = fuh->bytecode.load(); bytecode != nullptr) {
        return bytecode;
    }

    auto offset = def.GetCodeOffset();
    auto code   = Symlevel::Reader::Read(session, def.FileId(), offset);

    auto resolver = API::Resolver::Create(session, fuh->methodDef);

    if (Cbc::IsDisasmEnabled()) {
        Cbc::Disasm(Stream::cout, code, resolver.get())->ParseAll();
    }

    Cbc::Emitter::Emitter emitter;
    Cbc::Rewriter(*resolver, code, emitter)->ParseAll();

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

void* FunctionHandleManager::GetFunctionPtr(TaggedFunctionHandle fuh)
{
    if (auto* staticFuh = std::get_if<StaticFunctionHandle*>(&fuh)) {
        return (*staticFuh)->function;
    } else {
        auto dynFuh = std::get<DynamicFunctionHandle*>(fuh);
        return GetDirectCallTrampoline(dynFuh);
    }
}

} // namespace Interpretation
