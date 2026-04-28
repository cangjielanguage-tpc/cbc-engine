#pragma once

#include "RuntimeTypes.h"
#include "interpreter/function_handle.h"

namespace RTSupport {

struct CbcTypeInfo : public MRTExport::type_info_t {
    Interpretation::FunctionHandle** dataMT;
};

} // namespace RTSupport
