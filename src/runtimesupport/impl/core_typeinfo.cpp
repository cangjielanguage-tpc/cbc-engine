#include "runtimesupport/typeinfo_factory.h"
#include "engine/decode/reader.h"
#include "engine/image/flags.h"
#include "runtimesupport/impl/cjnative.h"
#include "utils/assertion.h"
#include "utils/sharedobj.h"
#include <dlfcn.h>

namespace RTSupport {

void InitCoreTypeInfoMetadata(Engine::Session& session)
{
    bool hasCbcCore = false;
    for (auto name : { "std.core:Any", "std.core:Object" }) {
        auto definition = session.GetEngine().FindType(session, name);
        if (!definition || Decode::Read(session, *definition).GetFlags().Is(Image::TypeFlag::AOT)) {
            continue;
        }
        hasCbcCore = true;
        if (g_CJNativeInterfaceInstance.typeInfo(name) != nullptr) {
            FATAL("cannot initialize CBC core type %s: native type metadata is already registered", name);
        }
    }
    if (!hasCbcCore) {
        return;
    }
    // Check all roots before construction, which can recursively register
    // another CBC root. Native roots cannot be replaced by this initializer.
    for (auto name : { "std.core:Any", "std.core:Object" }) {
        auto definition = session.GetEngine().FindType(session, name);
        if (!definition || Decode::Read(session, *definition).GetFlags().Is(Image::TypeFlag::AOT)) {
            continue;
        }
        auto term = Engine::TermManager::Of(session).NewAotTerm(session, name, {}, true);
        // AcquireTypeInfo completes UUID registration through the runtime API,
        // which inserts this real CBC definition into the runtime's name map.
        auto typeInfo = Engine::TypeInfoManager::Of(session).AcquireTypeInfo(session, term);
        if (!typeInfo) {
            FATAL("failed to build CBC core type info for %s", name);
        }
    }

    // The runtime normally initializes these caches after registering a native
    // file. CBC images do not use that loader, so initialize still-null caches,
    // without loading native std.core or depending on private object offsets.
    // These exported C++ entrypoints are an SDK ABI dependency until the runtime
    // exposes this operation in DYN_CJNativeInterface.
    Dl_info info {};
    if (dladdr(reinterpret_cast<void*>(g_CJNativeInterfaceInstance.stackGrowStub), &info) == 0) {
        FATAL("cannot locate runtime for CBC core type registration");
    }
    auto runtime = Utils::SharedObject::Open(info.dli_fname);
    if (!runtime.IsOpened() || runtime.SearchSym("CJ_MCC_StackGrowStub").symbol !=
            reinterpret_cast<void*>(g_CJNativeInterfaceInstance.stackGrowStub)) {
        FATAL("cannot reopen the active runtime for CBC core type registration");
    }
    using GetManager = void* (*)();
    using InitCore = void (*)(void*);
    auto getManager = reinterpret_cast<GetManager>(runtime.SearchSym(
        "_ZN12MapleRuntime15TypeInfoManager18GetTypeInfoManagerEv").symbol);
    auto initCore = reinterpret_cast<InitCore>(runtime.SearchSym(
        "_ZN12MapleRuntime15TypeInfoManager20InitAnyAndObjectTypeEv").symbol);
    if (getManager == nullptr || initCore == nullptr) {
        FATAL("runtime does not expose CBC core type cache initialization");
    }
    initCore(getManager());
}

} // namespace RTSupport
