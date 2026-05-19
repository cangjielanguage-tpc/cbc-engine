#pragma once

#include "RTInterface.h"
#include "RuntimeTypes.h"

namespace RTSupport {

void Initialize(DYN_CJNativeInterface* interf);

extern DYN_FuncPtr* (*GetMTable)(DYN_TypeInfo* t, DYN_TypeInfo* itf);
extern void (*UpdateVMT)(DYN_TypeInfo* t, DYN_TypeInfo* itf, DYN_ExtensionData* extData);
extern DYN_TypeInfo* (*GetMethodOuterTI)(DYN_TypeInfo* t, DYN_TypeInfo* itf, int index);

} // namespace RTSupport
