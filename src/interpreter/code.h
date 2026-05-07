#ifndef INTERPRETER_CODE_H
#define INTERPRETER_CODE_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include "asm_export.h"
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

struct ReferenceInfo {
    uint32_t rewrittenPos;
    uint16_t regMask;
    std::vector<uint32_t> refSlotOffsets;
};

struct ExecBytecodeInfo {
    Code const code;
    uint16_t const savedIRegs;
    uint16_t const savedFRegs;
    uint16_t const untypedSlotCount;
    uint32_t const frameSize;
    std::vector<ReferenceInfo> const referenceInfos;

    friend Stream::Output& operator<<(Stream::Output& out, const ExecBytecodeInfo& bc) {
        using namespace Stream;

        out << "ExecBytecodeInfo {"   << endl
            << "\tuntypedSlotCount: " << bc.untypedSlotCount << endl
            << "\tframeSize: "        << bc.frameSize << endl;
        {
            out << "\tGCMap {" << endl;
            for (auto& entry : bc.referenceInfos) {
                out << "\t\trtPos: " << entry.rewrittenPos
                    << ", regMask: " << entry.regMask
                    << ", "          << Std::Vector::ToString(entry.refSlotOffsets)
                    << endl;
            }
            out << "\t}" << endl;
        }

        return out << "}" << endl;
    } 
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
