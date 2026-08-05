#include "rt_syms.h"
#include "engine/engine.h"
#include "runtimesupport/impl/asm_trampolines.h"
#include "utils/logger.h"
#include "utils/ostream.h"
#include "utils/rt_logger.h"
#include <dlfcn.h>
#include <string_view>

namespace RTSupport {

DYN_WriteStructFieldFn WriteStructField;
DYN_ReadStructFieldFn ReadStructField;

DYN_WriteGenericFieldFn WriteGeneric;

// Prolongs lifetime of helper lib handle after finish of Initialize.
Utils::SharedObject g_helperLibHandle;

static void* LookupSymbol(void* handle, char const* handleName, char const* symbolName)
{
    dlerror();
    auto* symbol = dlsym(handle, symbolName);
    if (symbol == nullptr) {
        auto* error  = dlerror();
        auto& stream = Log::init.Stream(Logging::Level::ERROR);
        stream << '{' << handleName << "} failed to find " << symbolName;
        if (error != nullptr) {
            stream << ": " << error;
        }
        stream << Stream::endl;
    }
    return symbol;
}

bool Initialize(DYN_CJNativeInterface* interf)
{
    auto anySym = reinterpret_cast<void*>(interf->stackGrowStub);

    Dl_info info;
    int code = dladdr(anySym, &info);
    if (code == 0) {
        Log::init.Stream(Logging::Level::ERROR) << "dladdr failed to find rt info" << Stream::endl;
        return false;
    }
    auto handle = Utils::SharedObject::Open(std::string_view(info.dli_fname));
    if (!handle.IsOpened()) {
        Log::init.Stream(Logging::Level::ERROR) << "failed to open lib " << info.dli_fname << Stream::endl;
        return false;
    }

    // verify that we didn't opened new library (any other exported symbol can be used).
    auto stackGrowStub = handle.SearchSym("CJ_MCC_StackGrowStub");
    Asm::engine_newthread_nret_function =
        (decltype(Asm::engine_newthread_nret_function))handle.SearchSym("CJ_MCC_NewCJThreadNoReturn");

    Asm::engine_read_generic = interf->readGenericField;
    WriteGeneric             = interf->writeGenericField;

    WriteStructField = interf->writeStructField;
    ReadStructField  = interf->readStructField;

    if (stackGrowStub != interf->stackGrowStub) {
        auto& stream = Log::init.Stream(Logging::Level::ERROR);
        stream << "incorrect stack grow stub address ";
        stream << interf->stackGrowStub << " " << stackGrowStub << Stream::endl;
        return false;
    }

#if defined(__APPLE__) && defined(STATIC_HELPER)
    if (interf->appLibHandle == nullptr) {
        Log::init.Stream(Logging::Level::ERROR)
            << "STATIC_HELPER requires a non-null application library handle" << Stream::endl;
        return false;
    }

    auto* thrower = LookupSymbol(
        interf->appLibHandle, "application", "_CN32cangjie.runtime.cbcengine.helper22throwImplicitExceptionHl"
    );
    auto* spawnFuture = LookupSymbol(interf->appLibHandle, "application", "helper_spawn_future");
#else
    #if defined(__APPLE__)
    std::string_view helperLibName = "@rpath/libcbcengine-helper.dylib";
    #else
    std::string_view helperLibName = "libcbcengine-helper.so";
    #endif
    auto helperHandle = Utils::SharedObject::Open(helperLibName);
    if (!helperHandle.IsOpened()) {
        Log::init.Stream(Logging::Level::ERROR) << "failed to open lib " << helperLibName << Stream::endl;
        return false;
    }

    auto* thrower = helperHandle.SearchSym("_CN32cangjie.runtime.cbcengine.helper22throwImplicitExceptionHl");
    auto* spawnFuture = helperHandle.SearchSym("helper_spawn_future");
    g_helperLibHandle = std::move(helperHandle);
#endif

    if (thrower == nullptr || spawnFuture == nullptr) {
        Log::init.Stream(Logging::Level::ERROR) << "failed to find required cbcengine-helper symbols" << Stream::endl;
        return false;
    }

    Asm::engine_implicit_exception_thrower = reinterpret_cast<void (*)(int)>(thrower);
    Asm::engine_spawn_future               = spawnFuture;
    return true;
}

} // namespace RTSupport
