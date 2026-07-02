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

// List of non-zero registers used for storing non-volatile regs.
struct RegisterList {
    uint64_t value {};

    // Encode register list from given mask.
    explicit RegisterList(uint16_t mask)
    {
        // [reg_0: u4, reg_1: u4, .., reg_n: u4, zeros]
        uint64_t list   = 0;
        uint64_t reg    = 0;
        uint64_t p      = 1;
        uint64_t offset = 0;
        // Ascending order
        while (reg < 16) {
            if (mask & p) {
                list    = list | (reg << offset);
                offset += 4;
            }
            reg++;
            p <<= 1;
        }
        value = list;
    }

    // Extract one register from register list.
    uint8_t ExtractReg()
    {
        uint64_t result = value & 0xf;
        value           = value >> 4;
        return result;
    }

    bool IsEmpty() { return value == 0; }
};

struct ExecBytecodeInfo {
    Code const code;
    RegisterList savedIRegs;
    RegisterList savedFRegs;
    uint32_t const frameSize;
    uint16_t const untypedSlotCount;
    GcInfo const gcInfo;
    InstructionOffsetsIndex const offsetsIndex; // TODO: optimize RAM footprint

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
