#pragma once

#include "cbc/isa_parser.h"
#include "engine/engine.h"
#include "resolution/resolution.h"
#include "utils/logger.h"

namespace Cbc {

extern Logging::Logger log;

extern bool emitLogInstructions;

Interpretation::ExecBytecodeInfo Rewrite(MethodCode code, Resolution::Resolver& resolver, Memory::Heap& heap);
Interpretation::ExecBytecodeInfo Rewrite(
    Engine::Session& session, Image::Identifier<Image::MethodDefinition> method, Memory::Heap& heap
);

} // namespace Cbc
