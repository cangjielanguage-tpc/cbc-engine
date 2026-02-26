#include <mutex>
#include <unordered_map>

#include "cbc/rewriter.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/reader.h"
#include "function_handle.h"

namespace Interpretation {

class FunctionHandleManager::Impl {
public:
    std::mutex lock;
    std::unordered_map<Engine::Identifier<Symlevel::MethodDefinition>, FunctionHandle*> fuhMap;
};

FunctionHandleManager::FunctionHandleManager() : impl(std::move(std::make_unique<FunctionHandleManager::Impl>())) {}

FunctionHandleManager::~FunctionHandleManager()                               = default;
FunctionHandleManager::FunctionHandleManager(FunctionHandleManager&& manager) = default;

FunctionHandle* FunctionHandleManager::Acquire(
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

ExecBytecodeInfo* FunctionHandleManager::Prepare(Engine::Session& session, DynamicFunctionHandle* fuh)
{
    auto def = Symlevel::MethodDefinition::Resolve(session, fuh->methodDef);

    std::lock_guard guard(fuh->lock);

    if (auto desc = fuh->descriptor.load(); desc != nullptr) {
        return desc;
    }

    auto offset = def.GetCodeOffs();
    auto code   = Symlevel::Reader::Read(session, def.FileId(), offset);

    Emitter::Emitter emitter;
    Cbc::Rewriter rewriter(nullptr, code, emitter);
    rewriter.Interpret();

    auto& heap         = session.GetEngine().CodeHeap();
    auto rewrittenCode = emitter.Build(heap);

    ExecBytecodeInfo newDesc = {
        .code = rewrittenCode,
        // TODO: initialize rest
    };

    fuh->descriptor.store(new ExecBytecodeInfo(newDesc));

    // Return via reload from `fuh->descriptor` to guarantee proper memory-model semantics:
    // fields (and fields of fields) would be visible from other threads
    // if the content of desc or desc itself would be published through "relaxed" (or race) stores
    // (explicitly in the codebase, or implictly in ASM or interpreter).
    return fuh->descriptor.load();
}

} // namespace Interpretation
