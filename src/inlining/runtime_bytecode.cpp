#include "runtime_bytecode.h"

#include "cbc/emitter/encoding_rt.h"
#include "cbc/emitter/segment.h"
#include "engine/decode/decoder.h"
#include "interpreter/literals.h"
#include "utils/assertion.h"
#include "utils/math.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
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

bool IsSupportedInstruction(const Interpretation::Code& code, const RuntimeInstruction& instruction)
{
    using Opcode = RT::Opcode;
    Decoder::ByteReader reader(code.bytecode + instruction.offset, code.bytecode, code.bytecode + code.bytecodeSize);
    switch (instruction.opcode) {
        // Reference access in these families calls into the managed runtime.
        case Opcode::LOAD_OBJ:
        case Opcode::LOAD_OBJ_F:
        case Opcode::ATOMIC_LOAD: return RT::B4xi12rr::Decode(reader).xi12.imm4.LDK() != Format::LoadAccessKind::LD_REF;
        case Opcode::STORE_OBJ:
        case Opcode::STORE_OBJ_F:
        case Opcode::ATOMIC_STORE:
            return RT::B4xi12rr::Decode(reader).xi12.imm4.STK() != Format::StoreAccessKind::ST_REF;
        case Opcode::LOAD_ARR:
        case Opcode::LOAD_ARR_F:  return RT::B3xrrr::Decode(reader).xr.imm.LDK() != Format::LoadAccessKind::LD_REF;
        case Opcode::STORE_ARR:
        case Opcode::STORE_ARR_F: return RT::B3xrrr::Decode(reader).xr.imm.STK() != Format::StoreAccessKind::ST_REF;
        case Opcode::LOAD_LONG_DERIVED:
        case Opcode::LOAD_LONG_DERIVED_F:
            return RT::B7xrrri32::Decode(reader).xr.imm.LDK() != Format::LoadAccessKind::LD_REF;
        case Opcode::STORE_LONG_DERIVED:
        case Opcode::STORE_LONG_DERIVED_F:
            return RT::B7xrrri32::Decode(reader).xr.imm.STK() != Format::StoreAccessKind::ST_REF;
        case Opcode::LOAD_ADDR:  return RT::B4xri16::Decode(reader).xr.imm.LDK() != Format::LoadAccessKind::LD_REF;
        case Opcode::STORE_ADDR: return RT::B4xri16::Decode(reader).xr.imm.STK() != Format::StoreAccessKind::ST_REF;
        // Calls, allocations, safepoints and exceptional control flow need inline metadata.
        case Opcode::NEWOBJ:
        case Opcode::NEWOBJ_PINNED:
        case Opcode::NEWOBJ_G:
        case Opcode::NEWOBJ_PINNED_G:
        case Opcode::NEWOBJ_ACC_G:
        case Opcode::NEWARR:
        case Opcode::NEWARR_G:
        case Opcode::NEWBOX:
        case Opcode::NEWBOX2:
        case Opcode::SPAWN:
        case Opcode::SPAWN_FUTURE:
        case Opcode::LOAD_GENERIC:
        case Opcode::CALL_CLOSURE:
        case Opcode::CALL_CLOSURE_SRET:
        case Opcode::CALL_CLOSURE_GENERIC:
        case Opcode::DIRECT_CALL_2I:
        case Opcode::DIRECT_CALL_2C:
        case Opcode::DIRECT_CALL_2N:
        case Opcode::CALL_CFUNC:
        case Opcode::VIRTUAL_CALL:
        case Opcode::INTERFACE_CALL:
        case Opcode::INTERFACE_CALL_GENERIC:
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
        case Opcode::CHECKED_CONVERT:
        case Opcode::NULLCHECK:
        case Opcode::DIVCHECK:
        case Opcode::ARRAY_INDEX_CHECK:
        case Opcode::THROW:
        case Opcode::CATCH:                  return false;
        // Frame operands whose relocation or ABI semantics still need support.
        case Opcode::STRING_INIT:
        case Opcode::UNBOX_T:
        case Opcode::STK_PARAM:
        case Opcode::STK_PARAM_F:
        case Opcode::STK_PARAM_S16:
        case Opcode::STK_PARAM_S32:
        case Opcode::LEA_STACK_PARAM: return false;
        // Runtime helpers or managed barriers, including copies with a dynamic storage kind.
        case Opcode::STORE_GENERIC:
        case Opcode::STORE_GENERIC_1:
        case Opcode::STORE_GENERIC_4:
        case Opcode::INIT_SOME_GENERIC_OPT0:
        case Opcode::INIT_SOME_GENERIC_OPT1:
        case Opcode::LOAD_GENERIC_TI:
        case Opcode::IOF:
        case Opcode::IOF_GENERIC:
        case Opcode::ASSIGN_GENERIC:
        case Opcode::READ_STRUCT_FIELD:
        case Opcode::WRITE_STRUCT_FIELD:
        case Opcode::CAS_REF:
        case Opcode::ATOMIC_SWAP_REF:
        case Opcode::COPY_DERIVED:
        case Opcode::COPY_DERIVED_GENERIC:
        case Opcode::COPY_TO_LOCAL:
        case Opcode::COPY_FROM_LOCAL:
        case Opcode::COPY_TO_LOCAL_GENERIC:
        case Opcode::COPY_FROM_LOCAL_GENERIC:
        case Opcode::COPY_TO_TYPESLOT:
        case Opcode::COPY_FROM_TYPESLOT:
        case Opcode::COPY_TO_TYPESLOT_GENERIC:
        case Opcode::COPY_FROM_TYPESLOT_GENERIC: return false;
        case Opcode::HALT:
        case Opcode::OPCODE_NUM:                 return false;
        default:                                 return true;
    }
}

std::optional<RT::Opcode> LiteralBranchOpcode(RT::Opcode opcode)
{
    using Opcode = RT::Opcode;
    switch (opcode) {
        case Opcode::BCC16I:
        case Opcode::BCC16L:  return Opcode::BCC16L;
        case Opcode::BCC32I:
        case Opcode::BCC32L:  return Opcode::BCC32L;
        case Opcode::BCC64I:
        case Opcode::BCC64L:  return Opcode::BCC64L;
        case Opcode::BCCI32I:
        case Opcode::BCCI32L: return Opcode::BCCI32L;
        case Opcode::BCCI64I:
        case Opcode::BCCI64L: return Opcode::BCCI64L;
        case Opcode::BCCL32I:
        case Opcode::BCCL32L: return Opcode::BCCL32L;
        case Opcode::BCCL64I:
        case Opcode::BCCL64L: return Opcode::BCCL64L;
        default:              return std::nullopt;
    }
}

struct BranchDestination {
    int64_t offset;
    std::optional<uint32_t> literalIndex;
};

std::optional<BranchDestination> BranchTarget(const Interpretation::Code& code, const RuntimeInstruction& instruction)
{
    auto* const end = code.bytecode + code.bytecodeSize;
    Decoder::ByteReader reader(code.bytecode + instruction.offset, code.bytecode, end);

    uint32_t operand;
    switch (instruction.opcode) {
        case RT::Opcode::BCC16I:
        case RT::Opcode::BCC32I:
        case RT::Opcode::BCC64I:
        case RT::Opcode::BCC16L:
        case RT::Opcode::BCC32L:
        case RT::Opcode::BCC64L:        operand = RT::B4xi12rr::Decode(reader).xi12.imm12; break;
        case RT::Opcode::BCCI32I:
        case RT::Opcode::BCCI64I:
        case RT::Opcode::BCCI32L:
        case RT::Opcode::BCCI64L:
        case RT::Opcode::BCCL32I:
        case RT::Opcode::BCCL64I:
        case RT::Opcode::BCCL32L:
        case RT::Opcode::BCCL64L:       operand = RT::B5xi12ri12::Decode(reader).xi12.imm12; break;
        case RT::Opcode::JMP32:         operand = RT::B5i32::Decode(reader).imm32.imm; break;
        case RT::Opcode::BRANCH_IS_REF: operand = RT::B3xi12::Decode(reader).xi12.imm12; break;
        default:                        return std::nullopt;
    }
    const auto branchEnd = static_cast<int64_t>(instruction.offset) + instruction.size;
    if (LiteralBranchOpcode(instruction.opcode) == instruction.opcode) {
        if (operand >= code.literals->size()) {
            return std::nullopt;
        }
        const auto delta = code.literals->at(operand).i64;
        if (delta < -branchEnd || delta > std::numeric_limits<uint32_t>::max() - branchEnd) {
            return std::nullopt;
        }
        return BranchDestination { branchEnd + delta, operand };
    }
    const auto bits  = instruction.opcode == RT::Opcode::JMP32 ? 32 : 12;
    const auto delta = static_cast<int64_t>(MathUtils::SignExtend<uint64_t>(operand, bits));
    return BranchDestination { branchEnd + delta, std::nullopt };
}

bool IsInstructionBoundary(const std::vector<RuntimeInstruction>& instructions, int64_t offset)
{
    if (offset < 0 || offset > std::numeric_limits<uint32_t>::max()) {
        return false;
    }
    const auto instruction = std::lower_bound(
        instructions.begin(),
        instructions.end(),
        static_cast<uint32_t>(offset),
        [](const RuntimeInstruction& candidate, uint32_t target) { return candidate.offset < target; }
    );
    return instruction != instructions.end() && instruction->offset == static_cast<uint32_t>(offset);
}

bool IsSupportedBranch(
    const Interpretation::Code& code,
    const std::vector<RuntimeInstruction>& instructions,
    const RuntimeInstruction& instruction
)
{
    const auto target = BranchTarget(code, instruction);
    return target.has_value() && IsInstructionBoundary(instructions, target->offset);
}

RT::Opcode LongFrameOpcode(RT::Opcode opcode)
{
    switch (opcode) {
        case RT::Opcode::LOAD_FRAME:    return RT::Opcode::LOAD_LONG_FRAME;
        case RT::Opcode::LOAD_FRAME_F:  return RT::Opcode::LOAD_LONG_FRAME_F;
        case RT::Opcode::STORE_FRAME:   return RT::Opcode::STORE_LONG_FRAME;
        case RT::Opcode::STORE_FRAME_F: return RT::Opcode::STORE_LONG_FRAME_F;
        default:                        FATAL("Expected a short frame opcode");
    }
}

} // namespace

void EmitRelocatedInstruction(
    Emitter::Segment& output,
    const Interpretation::Code& code,
    const RuntimeInstruction& instruction,
    uint32_t frameShift,
    uint32_t literalBase
)
{
    auto* begin = code.bytecode + instruction.offset;
    Decoder::ByteReader reader(begin, code.bytecode, code.bytecode + code.bytecodeSize);
    const auto relocateOffset = [frameShift](int32_t offset) {
        const auto relocated = static_cast<int64_t>(offset) - frameShift;
        ASSERT(relocated >= std::numeric_limits<int32_t>::min());
        return static_cast<int32_t>(relocated);
    };
    const auto relocateImm32 = [&](Format::Imm32& operand) {
        operand.imm = static_cast<uint32_t>(relocateOffset(static_cast<int32_t>(operand.imm)));
    };
    using Opcode = RT::Opcode;
    switch (instruction.opcode) {
        case Opcode::BINI32L:
        case Opcode::BINI64L:
        case Opcode::SCCI32L:
        case Opcode::SCCI64L: {
            auto op = RT::B4xi12rr::Decode(reader);
            ASSERT(op.xi12.imm12 < code.literals->size());
            op.xi12.imm12 = static_cast<uint16_t>(op.xi12.imm12 + literalBase);
            Encode(output, op);
            break;
        }
        case Opcode::BCCL32I:
        case Opcode::BCCL64I:
        case Opcode::BCCL32L:
        case Opcode::BCCL64L: {
            auto op = RT::B5xi12ri12::Decode(reader);
            ASSERT(op.ri12.imm12 < code.literals->size());
            op.ri12.imm12 = static_cast<uint16_t>(op.ri12.imm12 + literalBase);
            Encode(output, op);
            break;
        }
        case Opcode::LOAD_ADDR:
        case Opcode::STORE_ADDR: {
            auto op = RT::B4xri16::Decode(reader);
            ASSERT(op.imm16.imm < code.literals->size());
            op.imm16.imm += literalBase;
            Encode(output, op);
            break;
        }
        case Opcode::LOAD_FRAME:
        case Opcode::LOAD_FRAME_F:
        case Opcode::STORE_FRAME:
        case Opcode::STORE_FRAME_F: {
            auto op     = RT::B4xi12rr::Decode(reader);
            auto offset = relocateOffset(MathUtils::SignExtend(static_cast<int32_t>(op.xi12.imm12), 12));
            if (MathUtils::IsNBitsSigned(offset, 12)) {
                op.xi12.imm12 = static_cast<uint16_t>(offset);
                Encode(output, op);
            } else {
                Encode(
                    output,
                    RT::B7xrrri32 {
                        .opc   = LongFrameOpcode(op.opc),
                        .xr    = { .imm = op.xi12.imm4, .r = op.rr.x },
                        .rr    = { .x = IReg::IRZ, .y = IReg::IRZ },
                        .imm32 = { .imm = static_cast<uint32_t>(offset) },
                    }
                );
            }
            break;
        }
        case Opcode::LOAD_LONG_FRAME:
        case Opcode::LOAD_LONG_FRAME_F:
        case Opcode::STORE_LONG_FRAME:
        case Opcode::STORE_LONG_FRAME_F: {
            auto op = RT::B7xrrri32::Decode(reader);
            relocateImm32(op.imm32);
            Encode(output, op);
            break;
        }
        case Opcode::STORE_FRAME_IMM_1:
        case Opcode::STORE_FRAME_IMM_2:
        case Opcode::STORE_FRAME_IMM_4:
        case Opcode::STORE_FRAME_IMM_8: {
            auto op = RT::StoreFrameImm::Decode(reader);
            op.offs = relocateOffset(op.offs);
            Encode(output, op);
            break;
        }
        case Opcode::COPY_TYPESLOT: {
            auto op = RT::CopyTypeSlot::Decode(reader);
            relocateImm32(op.dstOffset);
            relocateImm32(op.srcOffset);
            Encode(output, op);
            break;
        }
        case Opcode::COPY_TYPESLOT_GENERIC: {
            auto op = RT::CopyTypeSlotGeneric::Decode(reader);
            relocateImm32(op.dstOffset);
            relocateImm32(op.srcOffset);
            Encode(output, op);
            break;
        }
        case Opcode::COPY_TO_TYPESLOT: {
            auto op = RT::CopyToTypeSlot::Decode(reader);
            relocateImm32(op.dstOffset);
            Encode(output, op);
            break;
        }
        case Opcode::COPY_TO_TYPESLOT_GENERIC: {
            auto op = RT::CopyToTypeSlotGeneric::Decode(reader);
            relocateImm32(op.dstOffset);
            Encode(output, op);
            break;
        }
        case Opcode::COPY_FROM_TYPESLOT: {
            auto op = RT::CopyFromTypeSlot::Decode(reader);
            relocateImm32(op.srcOffset);
            Encode(output, op);
            break;
        }
        case Opcode::COPY_FROM_TYPESLOT_GENERIC: {
            auto op = RT::CopyFromTypeSlotGeneric::Decode(reader);
            relocateImm32(op.srcOffset);
            Encode(output, op);
            break;
        }
        case Opcode::UNBOX_T: {
            auto op = RT::B6xri32::Decode(reader);
            relocateImm32(op.imm32);
            Encode(output, op);
            break;
        }
        case Opcode::STRING_INIT: {
            auto op = RT::B13i64i32::Decode(reader);
            relocateImm32(op.imm32);
            Encode(output, op);
            break;
        }
        case Opcode::STK_PARAM_S16: {
            auto op        = RT::StackParamS16::Decode(reader);
            const auto src = relocateOffset(op.src);
            if (MathUtils::IsNBitsSigned(src, 16)) {
                op.src = static_cast<int16_t>(src);
                Encode(output, op);
            } else {
                Encode(output, RT::StackParamS32 { Opcode::STK_PARAM_S32, src, op.dst });
            }
            break;
        }
        case Opcode::STK_PARAM_S32: {
            auto op = RT::StackParamS32::Decode(reader);
            op.src  = relocateOffset(op.src);
            Encode(output, op);
            break;
        }
        case Opcode::ZERO_FRAME: {
            auto op   = RT::FrameRange::Decode(reader);
            op.offset = relocateOffset(op.offset);
            Encode(output, op);
            break;
        }
        default:
            for (auto* cursor = begin; cursor != begin + instruction.size; ++cursor) {
                output.AddW8(*cursor);
            }
            break;
    }
}

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

bool FitsBranchDelta(RT::Opcode opcode, int64_t delta)
{
    switch (opcode) {
        case RT::Opcode::BCC16I:
        case RT::Opcode::BCC32I:
        case RT::Opcode::BCC64I:
        case RT::Opcode::BCCI32I:
        case RT::Opcode::BCCI64I:
        case RT::Opcode::BCCL32I:
        case RT::Opcode::BCCL64I:
        case RT::Opcode::BRANCH_IS_REF: return MathUtils::IsNBitsSigned(delta, 12);
        case RT::Opcode::JMP32:         return MathUtils::IsNBitsSigned(delta, 32);
        default:                        return false;
    }
}

std::optional<std::vector<BranchPatch>> PrepareBranchPatches(
    const Interpretation::Code& code,
    const std::vector<RuntimeInstruction>& instructions,
    const std::vector<RewrittenCodeEdit>& edits,
    uint32_t literalBase,
    std::vector<Interpretation::Literal>& literals
)
{
    std::vector<BranchPatch> patches;
    for (const auto& instruction : instructions) {
        if (!IsRuntimeBranch(instruction.opcode)) {
            continue;
        }

        const auto oldTarget = BranchTarget(code, instruction);
        if (!oldTarget.has_value() || !IsInstructionBoundary(instructions, oldTarget->offset)) {
            return std::nullopt;
        }
        const auto newOffset = RelocateRewrittenPosition(instruction.offset, edits);
        const auto newTarget = RelocateRewrittenPosition(static_cast<uint32_t>(oldTarget->offset), edits);
        if (!newOffset.has_value() || !newTarget.has_value()) {
            return std::nullopt;
        }

        const auto newEnd = static_cast<int64_t>(*newOffset) + instruction.size;
        const auto delta  = static_cast<int64_t>(*newTarget) - newEnd;
        auto newOpcode    = instruction.opcode;
        auto operand      = delta;
        if (oldTarget->literalIndex.has_value() || !FitsBranchDelta(instruction.opcode, delta)) {
            const auto literalOpcode = LiteralBranchOpcode(instruction.opcode);
            if (!literalOpcode.has_value()) {
                return std::nullopt;
            }
            newOpcode = *literalOpcode;
            if (oldTarget->literalIndex.has_value() &&
                delta == oldTarget->offset - (static_cast<int64_t>(instruction.offset) + instruction.size)) {
                operand = literalBase + *oldTarget->literalIndex;
            } else {
                if (literals.size() >= RT::LIT_TABLE_SIZE) {
                    return std::nullopt;
                }
                // The old literal may also be used by other instructions.
                operand = static_cast<int64_t>(literals.size());
                literals.push_back({ .i64 = delta });
            }
        }
        patches.push_back(BranchPatch {
            .opcode    = instruction.opcode,
            .newOpcode = newOpcode,
            .offset    = *newOffset,
            .operand   = operand,
        });
    }
    return patches;
}

void ApplyBranchPatches(uint8_t* bytecode, uint32_t bytecodeSize, const std::vector<BranchPatch>& patches)
{
    for (const auto& patch : patches) {
        ASSERT(patch.offset < bytecodeSize);
        ASSERT(bytecode[patch.offset] == static_cast<uint8_t>(patch.opcode));
        bytecode[patch.offset] = static_cast<uint8_t>(patch.newOpcode);
        switch (patch.newOpcode) {
            case RT::Opcode::BCC16I:
            case RT::Opcode::BCC32I:
            case RT::Opcode::BCC64I:
            case RT::Opcode::BCC16L:
            case RT::Opcode::BCC32L:
            case RT::Opcode::BCC64L:
            case RT::Opcode::BCCI32I:
            case RT::Opcode::BCCI64I:
            case RT::Opcode::BCCI32L:
            case RT::Opcode::BCCI64L:
            case RT::Opcode::BCCL32I:
            case RT::Opcode::BCCL64I:
            case RT::Opcode::BCCL32L:
            case RT::Opcode::BCCL64L:
            case RT::Opcode::BRANCH_IS_REF: {
                ASSERT(bytecodeSize - patch.offset >= sizeof(uint8_t) + sizeof(uint16_t));
                uint16_t encoded;
                std::memcpy(&encoded, bytecode + patch.offset + sizeof(uint8_t), sizeof(encoded));
                encoded =
                    static_cast<uint16_t>((encoded & 0xfU) | ((static_cast<uint16_t>(patch.operand) & 0xfffU) << 4U));
                std::memcpy(bytecode + patch.offset + sizeof(uint8_t), &encoded, sizeof(encoded));
                break;
            }
            case RT::Opcode::JMP32: {
                ASSERT(bytecodeSize - patch.offset >= sizeof(uint8_t) + sizeof(uint32_t));
                const auto encoded = static_cast<uint32_t>(static_cast<int32_t>(patch.operand));
                std::memcpy(bytecode + patch.offset + sizeof(uint8_t), &encoded, sizeof(encoded));
                break;
            }
            default: FATAL("Unsupported runtime branch patch");
        }
    }
}

bool IsSupportedBody(const Interpretation::Code& code, const std::vector<RuntimeInstruction>& instructions)
{
    if (instructions.empty() || instructions.back().opcode != RT::Opcode::RET) {
        return false;
    }

    for (size_t i = 0; i < instructions.size(); ++i) {
        const auto opcode = instructions[i].opcode;
        if (!IsSupportedInstruction(code, instructions[i]) ||
            (opcode == RT::Opcode::RET && i + 1 != instructions.size()) ||
            (IsRuntimeBranch(opcode) && !IsSupportedBranch(code, instructions, instructions[i]))) {
            return false;
        }
    }
    return true;
}

bool HasSupportedInlineGC(
    const Interpretation::ExecBytecodeInfo& callee, const std::vector<RuntimeInstruction>& instructions
)
{
    const auto& maps = callee.gcInfo.positionalInfo;
    size_t nextMap   = 0;
    for (const auto& instruction : instructions) {
        if (instruction.opcode != RT::Opcode::GC_POINT) {
            continue;
        }
        if (nextMap == maps.Size()) {
            return false;
        }
        const auto& map = maps[nextMap++];
        if (map.rewrittenPos != instruction.offset + instruction.size || !map.paramRefSlots.Empty() ||
            !map.paramRecSlots.Empty()) {
            return false;
        }
    }
    return nextMap == maps.Size() && std::none_of(
                                         callee.stackPtrsInfo.positionalInfo.begin(),
                                         callee.stackPtrsInfo.positionalInfo.end(),
                                         [](const auto& map) { return !map.resources.Empty(); }
                                     );
}

std::optional<RelocatedInlineBody> RelocateInlineBody(
    const Interpretation::Code& code,
    const std::vector<RuntimeInstruction>& instructions,
    uint32_t frameShift,
    uint32_t literalBase,
    std::vector<Interpretation::Literal>& literals
)
{
    ASSERT(!instructions.empty() && instructions.back().opcode == RT::Opcode::RET);
    ASSERT(literalBase + code.literals->size() <= RT::LIT_TABLE_SIZE);
    Emitter::Segment output;
    std::vector<RewrittenCodeEdit> edits;
    for (size_t i = 0; i + 1 < instructions.size(); ++i) {
        const auto& instruction = instructions[i];
        const auto begin        = output.Pos();
        EmitRelocatedInstruction(output, code, instruction, frameShift, literalBase);
        const auto size = static_cast<uint32_t>(output.Pos() - begin);
        if (size != instruction.size) {
            edits.push_back({ instruction.offset, instruction.offset + instruction.size, size });
        }
    }
    auto body          = output.Finish();
    const auto patches = PrepareBranchPatches(code, instructions, edits, literalBase, literals);
    if (!patches.has_value()) {
        return std::nullopt;
    }
    ApplyBranchPatches(body.Data(), body.Size(), *patches);
    return RelocatedInlineBody { std::move(body), std::move(edits) };
}

} // namespace Cbc
