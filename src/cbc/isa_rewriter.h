#pragma once

#include <memory>

#include "api/resolver.h"
#include "emitter/emitter.h"
#include "isa_parser.h"

namespace Cbc {

std::unique_ptr<IsaParser> Rewriter(API::Resolver* resolver, MethodCode code, Emitter::Emitter& e);

} // namespace Cbc
