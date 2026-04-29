#pragma once

#include "RuntimeTypes.h"
#include "asm_export.h"
#include "interpreter/function_handle.h"
#include "runtimesupport/runtime.h"

namespace RTSupport {

struct CbcTypeInfo {
    DYN_TypeInfoT base;
    Interpretation::FunctionHandle** dataMT;
};

static_assert(offsetof(CbcTypeInfo, dataMT) == TYPEINFO_DATA_MT_OFFSET);

static DYN_TypeInfoT* UnpackTypeInfo(TypeInfo ti) { return reinterpret_cast<DYN_TypeInfoT*>(ti.Raw()); }

} // namespace RTSupport
