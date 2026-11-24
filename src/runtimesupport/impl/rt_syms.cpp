#include "rt_syms.h"
#include "runtimesupport/impl/asm_trampolines.h"
#include <dlfcn.h>

namespace RTSupport {

DYN_WriteStructFieldFn WriteStructField;
DYN_ReadStructFieldFn ReadStructField;

DYN_WriteGenericFieldFn WriteGeneric;
DYN_ReadWeakRefFn ReadWeakRef;

bool Initialize(DYN_CJNativeInterface* interf)
{
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
    Asm::engine_implicit_exception_thrower = reinterpret_cast<void (*)(int)>(implicitThrower);
    Asm::engine_future_execute_function = futureExecute;
    Asm::engine_set_runtime_cjthread_handle_function = setRuntimeCJThreadHandle;
}

} // namespace RTSupport
