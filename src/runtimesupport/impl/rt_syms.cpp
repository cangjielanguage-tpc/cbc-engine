#include "rt_syms.h"
#include "engine/engine.h"
#include "runtimesupport/impl/asm_trampolines.h"
#include "utils/logger.h"
#include "utils/ostream.h"
#include "utils/rt_logger.h"
#include <dlfcn.h>
#include <string_view>
#include <utility>

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

    if (stackGrowStub != interf->stackGrowStub) {
        auto& stream = Log::init.Stream(Logging::Level::ERROR);
        stream << "incorrect stack grow stub address ";
        stream << interf->stackGrowStub << " " << stackGrowStub << Stream::endl;
        return false;
    }

    const char* throwerName = "_CN32cangjie.runtime.cbcengine.helper22throwImplicitExceptionHl";
    const char* futureExecuteName = "_CNat6FutureIG_E7executeHv";
    const char* threadHandleSetterName  = "_CNat6Thread24setRuntimeCJThreadHandleHPu";

#if defined(__APPLE__) && defined(STATIC_HELPER)
    if (interf->appLibHandle == nullptr) {
        Log::init.Stream(Logging::Level::ERROR)
            << "STATIC_HELPER requires a non-null application library handle" << Stream::endl;
        return false;
    }

    auto throwerSym = LookupSymbol(interf->appLibHandle, "application", throwerName);
    auto futureExecuteSym = LookupSymbol(interf->appLibHandle, "application", futureExecuteName);
    auto threadHandleSetterSym = LookupSymbol(interf->appLibHandle, "application", threadHandleSetterName);
#else
    #if defined(__APPLE__)
    std::string_view helperLibName = "@rpath/libcbcengine-helper.dylib";
    #else
    std::string_view helperLibName = "libcbcengine-helper.so";
    #endif
    auto helperHandleOpt = Utils::SharedObject::Open(helperLibName);
    if (!helperHandleOpt.IsOpened()) {
        Log::init.Stream(Logging::Level::ERROR) << "failed to open lib " << helperLibName << Stream::endl;
        return false;
    }

    auto throwerSym = helperHandleOpt.SearchSym(throwerName);
    auto futureExecuteSym = helperHandleOpt.SearchSym(futureExecuteName);
    auto threadHandleSetterSym  = helperHandleOpt.SearchSym(threadHandleSetterName);

    g_helperLibHandle = std::move(helperHandleOpt);
#endif

    for (auto [name, symbol] : {
             std::pair { throwerName, throwerSym },
             std::pair { futureExecuteName, futureExecuteSym },
             std::pair { threadHandleSetterName, threadHandleSetterSym },
         }) {
        if (symbol == nullptr) {
            Log::init.Stream(Logging::Level::ERROR) << "failed to find symbol " << name << Stream::endl;
            return false;
        }
    }

    Asm::engine_newthread_nret_function             = interf->newCJThreadNoReturn;
    Asm::engine_read_generic                         = interf->readGenericField;
    Asm::engine_implicit_exception_thrower           = reinterpret_cast<void (*)(int)>(throwerSym);
    Asm::engine_future_execute_function              = futureExecuteSym;
    Asm::engine_set_runtime_cjthread_handle_function = threadHandleSetterSym;

    WriteGeneric     = interf->writeGenericField;
    WriteStructField = interf->writeStructField;
    ReadStructField  = interf->readStructField;
    return true;
}

} // namespace RTSupport
