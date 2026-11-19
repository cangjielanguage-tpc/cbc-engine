#pragma once

#include "cbc/isa_rt.h"
#include "cbc/offsets_index.h"
#include "interpreter/code.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace Cbc {

struct RuntimeInstruction {
    RT::Opcode opcode;
    uint32_t offset;
    uint32_t size;
};

struct BranchPatch {
    RT::Opcode opcode;
    uint32_t offset;
    int64_t delta;
};

std::vector<RuntimeInstruction> DecodeRuntimeBytecode(const Interpretation::Code& code);

bool IsRuntimeBranch(RT::Opcode opcode);

bool FitsBranchDelta(RT::Opcode opcode, int64_t delta);

std::optional<std::vector<BranchPatch>> PrepareBranchPatches(
    const Interpretation::Code& code,
    const std::vector<RuntimeInstruction>& instructions,
    const std::vector<RewrittenCodeEdit>& edits
);

void ApplyBranchPatches(uint8_t* bytecode, uint32_t bytecodeSize, const std::vector<BranchPatch>& patches);

bool IsSupportedBody(const Interpretation::Code& code);

} // namespace Cbc
