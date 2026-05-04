#pragma once

#include "interpreter/function_handle.h"
#include "isa_parser.h"
#include "resolution/resolution.h"
#include "utils/logger.h"

namespace Cbc {

extern Logging::Logger log;

Interpretation::ExecBytecodeInfo Rewrite(MethodCode code, Resolution::Resolver& resolver, Memory::Heap& heap);
Interpretation::ExecBytecodeInfo Rewrite(
    Interpretation::DynamicFunctionHandle* fuh, MethodCode code, Resolution::Resolver& resolver, Memory::Heap& heap
);

} // namespace Cbc
