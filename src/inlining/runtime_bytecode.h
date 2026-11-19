#pragma once

#include "cbc/isa_rt.h"
#include "interpreter/code.h"

#include <cstdint>
#include <vector>

namespace Cbc {

struct RuntimeInstruction {
    RT::Opcode opcode;
    uint32_t offset;
    uint32_t size;
};

std::vector<RuntimeInstruction> DecodeRuntimeBytecode(const Interpretation::Code& code);

bool IsRuntimeBranch(RT::Opcode opcode);

bool IsSupportedLinearBody(const Interpretation::Code& code);

} // namespace Cbc
