#include "runtime_bytecode.h"

#include "engine/decode/decoder.h"
#include "utils/assertion.h"

#include <cstddef>
#include <limits>

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

bool IsSupportedLinearInstruction(RT::Opcode opcode)
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
        case Opcode::BINI64I: return true;
        default:              return false;
    }
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

bool IsSupportedLinearBody(const Interpretation::Code& code)
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
        if (!IsSupportedLinearInstruction(opcode)) {
            return false;
        }
        if (opcode == RT::Opcode::RET && i + 1 != instructions.size()) {
            return false;
        }
    }
    return true;
}

} // namespace Cbc
