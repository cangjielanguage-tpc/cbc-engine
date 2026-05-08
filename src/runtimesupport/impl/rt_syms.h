#pragma once

#include "RTInterface.h"
#include "RuntimeTypes.h"

namespace RTSupport {

void Initialize(DYN_CJNativeInterfaceT* interf);

extern DYN_FuncPtrT* (*GetMTable)(DYN_TypeInfoT* t, DYN_TypeInfoT* itf);
extern void (*UpdateVMT)(DYN_TypeInfoT* t, DYN_TypeInfoT* itf, DYN_ExtensionDataT* extData);
extern DYN_TypeInfoT* (*GetMethodOuterTI)(DYN_TypeInfoT* t, DYN_TypeInfoT* itf, int index);

} // namespace RTSupport
