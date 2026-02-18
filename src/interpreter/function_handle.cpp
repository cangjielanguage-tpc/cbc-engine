#include <unordered_map>
#include <mutex>

#include "function_handle.h"
#include "engine/symlevel/method_definition.h"
#include "engine/symlevel/reader.h"
#include "cbc/rewriter.h"

namespace Interpretation {

std::mutex g_methodDefsLock;
std::unordered_map<Engine::MethodDefIdentifier, FunctionHandle*> g_handleMap;

FunctionHandle* AcquireFunctionHandle(Engine::Session& session, Engine::MethodDefIdentifier methodDef)
{
    std::lock_guard guard(g_methodDefsLock);
    auto res = g_handleMap.find(methodDef);
    if (res != g_handleMap.end()) {
        return res->second;
    }
    auto fuh = new DynamicFunctionHandle(
            nullptr,
            nullptr,
            nullptr,
            methodDef);
    g_handleMap[methodDef] = fuh;
    return fuh;
}

__attribute__((visibility ("default"))) FuHDescriptor* PrepareDynamicFuH(Engine::Session& session, DynamicFunctionHandle* fuh)
{
    auto& def = Symlevel::MethodDefinition::Resolve(session, fuh->methodDef);

    std::lock_guard guard(fuh->lock);

    auto desc = fuh->descriptor.load();
    if (desc) {
        return desc;
    }
    auto offset = def.GetCodeOffs();
    auto code = Symlevel::Reader::Read(session, def.FileId(), offset);

    Emitter::Emitter emitter;
    Cbc::Rewriter rewriter(nullptr, code, emitter);
    rewriter.Interpret();

    auto& heap = session.GetEngine().CodeHeap();
    auto rewrittenCode = emitter.Build(heap);

    FuHDescriptor newDesc = {
        .code = rewrittenCode,
        // TODO: initialize rest
    };

    desc = new FuHDescriptor(newDesc);
    fuh->descriptor.store(desc);

    // Return via reload from `fuh->descriptor` to guarantee proper memory-model semantics:
    // fields (and fields of fields) would be visible from other threads
    // if the content of desc or desc itself would be published through "relaxed" (or race) stores
    // (explicitly in the codebase, or implictly in ASM or interpreter).
    return fuh->descriptor.load();
}

} // namespace Interpretation
