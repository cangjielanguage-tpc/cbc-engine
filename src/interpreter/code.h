#ifndef INTERPRETER_CODE_H
#define INTERPRETER_CODE_H

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "asm_export.h"
#include "cbc/offsets_index.h"
#include "engine/engine.h"
#include "engine/terms.h"
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

struct Resource {
    uint32_t idx;

    bool IsReg() { return idx < Cbc::IReg::COUNT_ISA_ONLY; }

    IReg AsReg()
    {
        ASSERT(IsReg());
        return IReg(static_cast<IReg::Value>(idx));
    }

    uint32_t AsSlotNum()
    {
        ASSERT(!IsReg());
        return idx - Cbc::IReg::COUNT_ISA_ONLY;
    }
};

struct GCPositionalInfo {
    uint32_t rewrittenPos;
    uint16_t regMask;
    std::vector<uint32_t> untypedRefSlotsInfo;
    std::vector<std::pair<Resource, Resource>> mutPairs;
};

struct StackPtrsPositionalInfo {
    uint32_t rewrittenPos;
    std::vector<Resource> resources;
};

struct GcInfo {
    std::vector<GCPositionalInfo> positionalInfo;
    std::vector<uint32_t> refOffsets;
};

struct StackPtrsInfo {
    std::vector<StackPtrsPositionalInfo> positionalInfo;
};

// List of non-zero registers used for storing non-volatile regs.
struct NonVolatileRegs {
    static constexpr int REG_DATA_SIZE = 4;
    static constexpr int REGS_MAX_NUM  = 16;
    uint64_t value {};

    static_assert(REG_DATA_SIZE * REGS_MAX_NUM == 8 * sizeof(value));

    // Encode register list from given mask.
    explicit NonVolatileRegs(uint16_t mask)
    {
        // [reg_0: u4, reg_1: u4, .., reg_n: u4, zeros]
        uint64_t list   = 0;
        uint64_t reg    = 0;
        uint64_t p      = 1;
        uint64_t offset = 0;
        // Ascending order
        while (reg < REGS_MAX_NUM) {
            if (mask & p) {
                list    = list | (reg << offset);
                offset += REG_DATA_SIZE;
            }
            reg++;
            p <<= 1;
        }
        value = list;
    }

    // Extract one register from register list.
    uint8_t ExtractReg()
    {
        uint64_t result = value & (REGS_MAX_NUM - 1);
        value           = value >> REG_DATA_SIZE;
        return result;
    }

    bool IsEmpty() { return value == 0; }
};

struct AbiInfo {
    // Bitmap of all parameters (including sret) that point to stack.
    uint16_t stackPtrParams;
    // Bitmap of all reference parameters.
    uint16_t referenceParams;
    // Bitmap of all parameters which are represented as (base, derived) pairs.
    // N-th bit set => (base: N+1-th param, derived: N-th param)
    uint16_t derivedPairs;

    // Amount of parameters being passed by registers
    uint8_t iregParamCount;
    uint8_t fregParamCount;

    bool isSRet;
    bool hasTailReg;
};

struct ExecBytecodeInfo {
    Code code;
    NonVolatileRegs savedIRegs;
    NonVolatileRegs savedFRegs;
    uint32_t frameSize;
    uint16_t untypedSlotCount;
    AbiInfo abiInfo;
    GcInfo gcInfo;
    StackPtrsInfo stackPtrsInfo;
    InstructionOffsetsIndex offsetsIndex; // TODO: optimize RAM footprint

    friend Stream::Output& operator<<(Stream::Output& out, const ExecBytecodeInfo& bc);
};

struct AbiInfoFlags {
    bool isSRet : 1;
    bool isMut : 1;
    bool hasThisTypeInfo : 1;
    bool hasOuterTi : 1;
    bool recordReceiver : 1;
    bool referenceReceiver : 1;
    int funcVars;
};

AbiInfo BuildAbiInfo(Engine::Session& session, Engine::Term signature, AbiInfoFlags flags);

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
