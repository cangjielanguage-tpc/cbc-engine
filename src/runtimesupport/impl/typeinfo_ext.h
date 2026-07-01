#pragma once

#include "RuntimeTypes.h"
#include "asm_export.h"
#include "interpreter/function_handle.h"
#include "runtimesupport/runtime.h"

namespace RTSupport {

struct CbcTypeInfo {
    DYN_TypeInfo base;
    Interpretation::FunctionHandle** dataMT;
};

static_assert(offsetof(CbcTypeInfo, dataMT) == TYPEINFO_DATA_MT_OFFSET);
static_assert(offsetof(DYN_TypeInfo, instanceSize) == TYPEINFO_INSTANCESIZE_OFFSET);

static DYN_TypeInfo* UnpackTypeInfo(TypeInfo ti) { return reinterpret_cast<DYN_TypeInfo*>(ti.Raw()); }

} // namespace RTSupport
