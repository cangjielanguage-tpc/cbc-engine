#ifndef INTERPRETER_CODE_H
#define INTERPRETER_CODE_H

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "asm_export.h"
#include "cbc/offsets_index.h"
#include "literals.h"
#include "utils/misc.h"
#include "utils/ostream.h"

namespace Interpretation {

struct Code {
    std::size_t const bytecodeSize;
    uint8_t* const bytecode;
    Interpretation::LiteralTable* const literals;
    // TODO: add offsets converter
};

struct PositionalInfo {
    uint32_t rewrittenPos;
    uint16_t regMask;
    std::vector<uint32_t> untypedRefSlotsInfo;
};

struct GcInfo {
    std::vector<PositionalInfo> positionalInfo;
    std::vector<std::pair<uint32_t, void*>> typedSlotsInfo;
};

struct ExecBytecodeInfo {
    Code const code;
    uint16_t const savedIRegs;
    uint16_t const savedFRegs;
    uint16_t const untypedSlotCount;
    uint32_t const frameSize;
    GcInfo const gcInfo;
    InstructionOffsetsIndex const offsetsIndex;

    friend Stream::Output& operator<<(Stream::Output& out, const ExecBytecodeInfo& bc);
};

static_assert(
    offsetof(ExecBytecodeInfo, code) + offsetof(Code, bytecodeSize) == EXEC_BYTECODE_INFO_BYTECODE_SIZE_OFFSET
);
static_assert(offsetof(ExecBytecodeInfo, code) + offsetof(Code, bytecode) == EXEC_BYTECODE_INFO_BYTECODE_OFFSET);
static_assert(offsetof(ExecBytecodeInfo, code) + offsetof(Code, literals) == EXEC_BYTECODE_INFO_LITERALS_OFFSET);
static_assert(offsetof(ExecBytecodeInfo, savedIRegs) == EXEC_BYTECODE_INFO_SAVED_IREGS_OFFSET);
static_assert(offsetof(ExecBytecodeInfo, savedFRegs) == EXEC_BYTECODE_INFO_SAVED_FREGS_OFFSET);
static_assert(offsetof(ExecBytecodeInfo, frameSize) == EXEC_BYTECODE_INFO_FRAME_SIZE_OFFSET);

} // namespace Interpretation

#endif // INTERPRETER_CODE_H
