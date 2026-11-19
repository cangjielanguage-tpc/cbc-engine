#include "runtime_bytecode.h"

#include "engine/decode/decoder.h"
#include "utils/assertion.h"
#include "utils/math.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>

namespace Cbc {
namespace {

void DecodeAndAdvance(RT::Opcode opcode, Decoder::ByteReader& reader)
{
#define DECODE_INSTRUCTION(opcodeName, encoding, stringFormat)                                                         \
    case RT::Opcode::opcodeName: static_cast<void>(RT::encoding::Decode(reader)); return;

    switch (opcode) {
        CBC_RT_OPCODES(DECODE_INSTRUCTION)
        default: FATAL("Unknown runtime opcode while decoding bytecode");
    }

#undef DECODE_INSTRUCTION
}

bool IsSupportedInstruction(RT::Opcode opcode)
{
    using Opcode = RT::Opcode;
    switch (opcode) {
        case Opcode::RET:
        case Opcode::NOP:
        case Opcode::MOV:
        case Opcode::MOVI:
        case Opcode::BIN32:
        case Opcode::BIN64:
        case Opcode::BINI32I:
        case Opcode::BINI64I:
        case Opcode::CBIN8:
        case Opcode::CBIN16:
        case Opcode::CBIN32:
        case Opcode::CBIN64:
        case Opcode::CBINI8I:
        case Opcode::CBINI16I:
        case Opcode::CBINI32I:
        case Opcode::CBINI64I:
        case Opcode::CBINI8W:
        case Opcode::CBINI16W:
        case Opcode::CBINI32W:
        case Opcode::CBINI64W:
        case Opcode::SBIN8:
        case Opcode::SBIN16:
        case Opcode::SBIN32:
        case Opcode::SBIN64:
        case Opcode::SBINIMM8:
        case Opcode::SBINIMM16:
        case Opcode::SBINIMM32:
        case Opcode::SBINIMM64:
        case Opcode::SBINIMM8W:
        case Opcode::SBINIMM16W:
        case Opcode::SBINIMM32W:
        case Opcode::SBINIMM64W:
        case Opcode::FMOV:
        case Opcode::FMOVI16:
        case Opcode::FMOVI32:
        case Opcode::FMOVI64:
        case Opcode::FBIN16:
        case Opcode::FBIN32:
        case Opcode::FBIN64:
        case Opcode::FUN16:
        case Opcode::FUN32:
        case Opcode::FUN64:
        case Opcode::BCC32I:
        case Opcode::BCC64I:
        case Opcode::BCCI32I:
        case Opcode::BCCI64I:
        case Opcode::JMP32: return true;
        default:              return false;
    }
}

std::optional<int64_t> BranchTarget(const Interpretation::Code& code, const RuntimeInstruction& instruction)
{
    auto* const end = code.bytecode + code.bytecodeSize;
    Decoder::ByteReader reader(code.bytecode + instruction.offset, code.bytecode, end);

    int64_t delta;
    switch (instruction.opcode) {
        case RT::Opcode::BCC32I:
        case RT::Opcode::BCC64I: {
            const auto branch = RT::B4xi12rr::Decode(reader);
            delta = static_cast<int64_t>(MathUtils::SignExtend<uint64_t>(branch.xi12.imm12, 12));
            break;
        }
        case RT::Opcode::BCCI32I:
        case RT::Opcode::BCCI64I: {
            const auto branch = RT::B5xi12ri12::Decode(reader);
            delta = static_cast<int64_t>(MathUtils::SignExtend<uint64_t>(branch.xi12.imm12, 12));
            break;
        }
        case RT::Opcode::JMP32: {
            const auto branch = RT::B5i32::Decode(reader);
            delta = static_cast<int64_t>(MathUtils::SignExtend<uint64_t>(branch.imm32.imm, 32));
            break;
        }
        default: return std::nullopt;
    }
    return static_cast<int64_t>(instruction.offset) + instruction.size + delta;
}

bool IsSupportedBranch(
    const Interpretation::Code& code,
    const std::vector<RuntimeInstruction>& instructions,
    const RuntimeInstruction& instruction,
    uint32_t bodyEnd
)
{
    const auto target = BranchTarget(code, instruction);
    if (!target.has_value() || *target < 0 || *target > bodyEnd) {
        return false;
    }
    const auto targetInstruction = std::lower_bound(
        instructions.begin(),
        instructions.end(),
        static_cast<uint32_t>(*target),
        [](const auto& candidate, uint32_t offset) { return candidate.offset < offset; }
    );
    return targetInstruction != instructions.end() && targetInstruction->offset == static_cast<uint32_t>(*target);
}

} // namespace

std::vector<RuntimeInstruction> DecodeRuntimeBytecode(const Interpretation::Code& code)
{
    ASSERT(code.bytecodeSize <= std::numeric_limits<uint32_t>::max());

    std::vector<RuntimeInstruction> instructions;
    auto* const end = code.bytecode + code.bytecodeSize;
    Decoder::ByteReader reader(code.bytecode, code.bytecode, end);
    while (!reader.EndOfMem(end)) {
        const auto begin  = static_cast<uint32_t>(reader.Cursor() - code.bytecode);
        const auto opcode = RT::Opcode(reader.PeekOpcode());
        DecodeAndAdvance(opcode, reader);
        const auto instructionEnd = static_cast<uint32_t>(reader.Cursor() - code.bytecode);
        instructions.push_back(RuntimeInstruction { opcode, begin, instructionEnd - begin });
    }
    return instructions;
}

bool IsRuntimeBranch(RT::Opcode opcode)
{
    using Opcode = RT::Opcode;
    switch (opcode) {
        case Opcode::BCC16I:
        case Opcode::BCC32I:
        case Opcode::BCC64I:
        case Opcode::BCC16L:
        case Opcode::BCC32L:
        case Opcode::BCC64L:
        case Opcode::BCCI32I:
        case Opcode::BCCI64I:
        case Opcode::BCCI32L:
        case Opcode::BCCI64L:
        case Opcode::BCCL32I:
        case Opcode::BCCL64I:
        case Opcode::BCCL32L:
        case Opcode::BCCL64L:
        case Opcode::JMP32:
        case Opcode::BRANCH_IS_REF: return true;
        default:                    return false;
    }
}

bool IsSupportedBody(const Interpretation::Code& code)
{
    if (code.literals->size() != 0) {
        return false;
    }

    const auto instructions = DecodeRuntimeBytecode(code);
    if (instructions.empty() || instructions.back().opcode != RT::Opcode::RET) {
        return false;
    }

    for (size_t i = 0; i < instructions.size(); ++i) {
        const auto opcode = instructions[i].opcode;
        if (!IsSupportedInstruction(opcode)) {
            return false;
        }
        if (opcode == RT::Opcode::RET && i + 1 != instructions.size()) {
            return false;
        }
        if (IsRuntimeBranch(opcode) &&
            !IsSupportedBranch(code, instructions, instructions[i], instructions.back().offset)) {
            return false;
        }
    }
    return true;
}

} // namespace Cbc
