#pragma once

#include "RuntimeTypes.h"
#include "asm_export.h"
#include "interpreter/function_handle.h"
#include "runtimesupport/runtime.h"

namespace RTSupport {

struct CbcTypeInfo {
    MRTExport::type_info_t base;
    Interpretation::FunctionHandle** dataMT;
};

static_assert(offsetof(CbcTypeInfo, dataMT) == TYPEINFO_DATA_MT_OFFSET);

static MRTExport::type_info_t* UnpackTypeInfo(TypeInfo ti)
{
    return reinterpret_cast<MRTExport::type_info_t*>(ti.Raw());
}

} // namespace RTSupport
