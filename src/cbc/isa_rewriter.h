#pragma once

#include <memory>

#include "api/resolver.h"
#include "emitter/emitter.h"
#include "isa_parser.h"

namespace Cbc {

Interpretation::ExecBytecodeInfo Rewrite(MethodCode code, API::Resolver& resolver, Memory::Heap& heap);

} // namespace Cbc
