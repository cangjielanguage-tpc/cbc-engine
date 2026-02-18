#include <unordered_map>
#include <mutex>

#include "function_handle.h"
#include "engine/symlevel/method_definition.h"
#include "engine/symlevel/reader.h"

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

FuHDescriptor* PrepareDynamicFuH(Engine::Session& session, DynamicFunctionHandle* fuh)
{
    auto def = static_cast<Symlevel::MethodDefinition*>(fuh->methodDef);

    std::lock_guard guard(fuh->lock);
    auto desc = fuh->descriptor.load();
    if (desc) {
        return desc;
    }
    auto offset = def->GetCodeOffs();
    auto code = Symlevel::Reader::Read(session, def->FileId(), offset);
    // TODO: rewrite code and construct descriptor
    return nullptr;
}

} // namespace Interpretation
