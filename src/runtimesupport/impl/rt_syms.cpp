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

void Initialize(DYN_CJNativeInterface* interf)
{
    auto anySym = reinterpret_cast<void*>(interf->stackGrowStub);

    Dl_info info;
    int code = dladdr(anySym, &info);
    if (code == 0) {
        Log::init.Stream(Logging::Level::ERROR) << "dladdr failed to find rt info" << Stream::endl;
        return;
    }
    void* base  = info.dli_fbase;
    auto handle = Utils::SharedObject::Open(std::string_view(info.dli_fname));
    if (!handle.IsOpened()) {
        Log::init.Stream(Logging::Level::ERROR) << "failed to open lib " << info.dli_fname << Stream::endl;
        return;
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
        return;
    }

#if defined(__APPLE__)
    std::string_view helperLibName = "libcbcengine-helper.dylib";
#else
    std::string_view helperLibName = "libcbcengine-helper.so";
#endif
    auto helperHandleOpt = Utils::SharedObject::Open(helperLibName);
    if (!helperHandleOpt.IsOpened()) {
        Log::init.Stream(Logging::Level::ERROR) << "failed to open lib " << helperLibName << Stream::endl;
        return;
    }

    const char* throwerName = "_CN32cangjie.runtime.cbcengine.helper22throwImplicitExceptionHl";
    auto throwerSym         = helperHandleOpt.SearchSym(throwerName);
    if (throwerSym == nullptr) {
        Log::init.Stream(Logging::Level::ERROR) << "failed to find symbol " << throwerName << Stream::endl;
        return;
    }

    g_helperLibHandle                      = std::move(helperHandleOpt);
    Asm::engine_implicit_exception_thrower = reinterpret_cast<void (*)(int)>(throwerSym);
    Asm::engine_spawn_future               = g_helperLibHandle.SearchSym("helper_spawn_future");
}

} // namespace RTSupport
