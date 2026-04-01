#include "method_impl.h"
#include "api/term.h"
#include "engine/symlevel/reader.h"
#include "engine/symlevel/region_data.h"

#include <dlfcn.h>

namespace API {
namespace Impl {

//////////////////////////////////
// DirectMethodCbc

Term* DirectMethodCbc::ABISignature()
{
    ASSERTION(false, "not implemented yet");
    return nullptr;
}

std::optional<Type*> DirectMethodCbc::RefType()
{
    ASSERTION(false, "not implemented yet");
    return nullptr;
}

std::optional<Interpretation::FunctionHandle*> DirectMethodCbc::FUH()
{
    auto& manager = Interpretation::FunctionHandleManager::Of(session);
    return manager.Acquire(session, def.GetIdentifier());
}

void* DirectMethodCbc::TargetAddr() { return nullptr; }

MethodFlags DirectMethodCbc::Flags()
{
    ASSERTION(false, "not implemented yet");
    return MethodFlags();
}

Symlevel::String DirectMethodCbc::Name() { return Symlevel::Reader::Read(session, def.FileId(), def.NameOffset()); }

//////////////////////////////////
// DirectMethodAot

Term* DirectMethodAot::ABISignature()
{
    ASSERTION(false, "not implemented yet");
    return nullptr;
}

std::optional<Type*> DirectMethodAot::RefType()
{
    ASSERTION(false, "not implemented yet");
    return nullptr;
}

std::optional<Interpretation::FunctionHandle*> DirectMethodAot::FUH() { return std::nullopt; }

void* DirectMethodAot::TargetAddr()
{
    auto linkageName = aotData.GetLinkageName();

    // TODO: manage libs
    void* handler = dlopen("libcangjie-std-core.so", RTLD_NOW);
    ASSERTION(handler != nullptr, "cannot open \"libcangjie-std-core.so\"");

    // TODO: manage nullptr
    void* target = dlsym(handler, std::string(linkageName).c_str());
    ASSERTION(target != nullptr, "cannot resolve target addt for direct aot call");

    dlclose(handler);

    return target;
}

MethodFlags DirectMethodAot::Flags()
{
    ASSERTION(false, "not implemented yet");
    return MethodFlags();
}

Symlevel::String DirectMethodAot::Name() { return ref.Name(); }

} // namespace Impl
} // namespace API
