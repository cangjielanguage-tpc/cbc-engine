#pragma once

#include "api/resolver.h"
#include "interpreter/function_handle.h"
#include "isa_parser.h"

namespace Cbc {

Interpretation::ExecBytecodeInfo Rewrite(MethodCode code, API::Resolver& resolver, Memory::Heap& heap);
Interpretation::ExecBytecodeInfo Rewrite(
    Interpretation::DynamicFunctionHandle* fuh, MethodCode code, API::Resolver& resolver, Memory::Heap& heap
);

} // namespace Cbc
