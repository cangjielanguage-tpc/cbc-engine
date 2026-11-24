#include "rt_syms.h"
#include "runtimesupport/impl/asm_trampolines.h"
#include "utils/logger.h"
#include "utils/ostream.h"
#include "utils/rt_logger.h"
#include "utils/sharedobj.h"
#include <dlfcn.h>
#include <string_view>
#include <utility>

namespace RTSupport {

DYN_WriteStructFieldFn WriteStructField;
DYN_ReadStructFieldFn ReadStructField;

DYN_WriteGenericFieldFn WriteGeneric;
DYN_ReadWeakRefFn ReadWeakRef;

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
    auto stackGrowStub = handle.SearchSym("CJ_MCC_StackGrowStub").symbol;

    if (stackGrowStub != interf->stackGrowStub) {
        auto& stream = Log::init.Stream(Logging::Level::ERROR);
        stream << "incorrect stack grow stub address ";
        stream << interf->stackGrowStub << " " << stackGrowStub << Stream::endl;
        return false;
    }

    Asm::engine_newthread_nret_function = interf->newCJThreadNoReturn;
    Asm::engine_read_generic            = interf->readGenericField;

    WriteGeneric     = interf->writeGenericField;
    WriteStructField = interf->writeStructField;
    ReadStructField  = interf->readStructField;
    return true;
}

void InitializeStaging(void* readWeakRef)
{
    ReadWeakRef = reinterpret_cast<DYN_ReadWeakRefFn>(readWeakRef);
}

void InstallBuiltins(void* implicitThrower, void* futureExecute, void* setRuntimeCJThreadHandle)
{
    Asm::engine_implicit_exception_thrower           = reinterpret_cast<void (*)(int)>(implicitThrower);
    Asm::engine_future_execute_function              = futureExecute;
    Asm::engine_set_runtime_cjthread_handle_function = setRuntimeCJThreadHandle;
}

} // namespace RTSupport
