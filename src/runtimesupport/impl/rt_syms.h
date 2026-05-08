#pragma once

#include "RTInterface.h"
#include "RuntimeTypes.h"

namespace RTSupport {

void Initialize(DYN_CJNativeInterfaceT* interf);

extern DYN_FuncPtrT* (*GetMTable)(DYN_TypeInfoT*, DYN_TypeInfoT*);

} // namespace RTSupport
