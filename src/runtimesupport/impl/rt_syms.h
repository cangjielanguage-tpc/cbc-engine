#pragma once

#include "RTInterface.h"
#include "RuntimeTypes.h"

namespace RTSupport {

bool Initialize(DYN_CJNativeInterface* interf);

extern DYN_WriteStructFieldFn WriteStructField;
extern DYN_ReadStructFieldFn ReadStructField;

extern DYN_WriteGenericFieldFn WriteGeneric;

} // namespace RTSupport
