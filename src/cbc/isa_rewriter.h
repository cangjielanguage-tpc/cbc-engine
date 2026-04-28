#pragma once

#include "api/resolver.h"
#include "isa_parser.h"

namespace Cbc {

Interpretation::ExecBytecodeInfo Rewrite(MethodCode code, API::Resolver& resolver, Memory::Heap& heap);

} // namespace Cbc
