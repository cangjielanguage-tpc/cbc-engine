#pragma once

#include "engine/engine.h"
#include "engine/symlevel/definitions.h"
#include "isa_parser.h"
#include "resolution/resolution.h"
#include "utils/logger.h"

namespace Cbc {

extern Logging::Logger log;

extern bool emitLogInstructions;

Interpretation::ExecBytecodeInfo Rewrite(MethodCode code, Resolution::Resolver& resolver, Memory::Heap& heap);
Interpretation::ExecBytecodeInfo Rewrite(
    Engine::Session& session, Engine::Identifier<Symlevel::MethodDefinition> method, Memory::Heap& heap
);

} // namespace Cbc
