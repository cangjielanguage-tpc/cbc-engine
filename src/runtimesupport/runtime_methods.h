#pragma once

#include <dlfcn.h>

#include "RuntimeTypes.h"
#include "runtime.h"

namespace RTMethods {

using TInfo      = MRTExport::type_info_t*;
using func_ptr_t = MRTExport::func_ptr_t;

using getMTable_t = func_ptr_t* (*)(TInfo, TInfo);
getMTable_t GetMTable();

void Init();

} // namespace RTMethods
