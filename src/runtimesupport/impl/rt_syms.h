#pragma once

#include "RTInterface.h"
#include "RuntimeTypes.h"

namespace RTSupport {

bool Initialize(DYN_CJNativeInterface* interf);

extern DYN_WriteStructFieldFn WriteStructField;
extern DYN_ReadStructFieldFn ReadStructField;

extern DYN_WriteGenericFieldFn WriteGeneric;

typedef DYN_ObjRef (*DYN_ReadWeakRefFn)(DYN_ObjRef source, DYN_FieldRef field);
extern DYN_ReadWeakRefFn ReadWeakRef;

} // namespace RTSupport
