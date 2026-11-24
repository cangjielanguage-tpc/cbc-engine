#pragma once

#include "RTInterface.h"
#include "RuntimeTypes.h"

namespace RTSupport {

/// Binds the runtime-provided symbols that arrive through `DYN_CJNativeInterface`.
/// Verifies the runtime library identity and installs the interface-derived hooks.
bool Initialize(DYN_CJNativeInterface* interf);

/// Installs the staging symbols that are not yet a part of `DYN_CJNativeInterface`
/// (see `engine_initialize_rt_staging` in cbc_engine.h).
void InitializeStaging(void* readWeakRef);

/// Installs the std library builtins (see `CjBuiltins` in cbc_engine.h).
void InstallBuiltins(void* implicitThrower, void* futureExecute, void* setRuntimeCJThreadHandle);

extern DYN_WriteStructFieldFn WriteStructField;
extern DYN_ReadStructFieldFn ReadStructField;

extern DYN_WriteGenericFieldFn WriteGeneric;

typedef DYN_ObjRef (*DYN_ReadWeakRefFn)(DYN_ObjRef source, DYN_FieldRef field);
extern DYN_ReadWeakRefFn ReadWeakRef;

} // namespace RTSupport
