#pragma once

#include "cbc/isa_parser.h"
#include "engine/engine.h"
#include "resolution/resolution.h"
#include "utils/logger.h"

namespace Cbc {

extern Logging::Logger log;

Interpretation::ExecBytecodeInfo Rewrite(MethodCode code, Resolution::Resolver& resolver, Memory::Heap& heap);
Interpretation::ExecBytecodeInfo Rewrite(
    Engine::Session& session, Symlevel::Identifier<Symlevel::MethodDefinition> method, Memory::Heap& heap
);

} // namespace Cbc
