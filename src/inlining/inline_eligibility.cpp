#include "inline_eligibility.h"

#include "cbc/isa_rt.h"
#include "engine/decode/reader.h"
#include "engine/image/flags.h"
#include "interpreter/function_handle.h"
#include "utils/assertion.h"
#include "utils/ostream.h"

namespace Cbc {
namespace {

bool IsSupportedInstruction(RT::Opcode opcode)
{
    using Opcode = RT::Opcode;
    switch (opcode) {
        case Opcode::RET:
        case Opcode::NOP:
        case Opcode::MOV:
        case Opcode::MOVI:
        case Opcode::FMOV:
        case Opcode::MOVI2F:
        case Opcode::MOVF2I:
        case Opcode::FMOVI32:
        case Opcode::FMOVI64:
        case Opcode::BCC32I:
        case Opcode::BCC64I:
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
        case Opcode::BIN32:
        case Opcode::BIN64:
        case Opcode::BINI32I:
        case Opcode::BINI64I:
        case Opcode::BINI32L:
        case Opcode::BINI64L:
        case Opcode::FBIN32:
        case Opcode::FBIN64:
        case Opcode::FUN32:
        case Opcode::FUN64:
        case Opcode::FMATHUN32:
        case Opcode::FMATHUN64:
        case Opcode::SCC32:
        case Opcode::SCC64:
        case Opcode::FSCC32:
        case Opcode::FSCC64:
        case Opcode::SCCI32I:
        case Opcode::SCCI64I:
        case Opcode::SCCI32L:
        case Opcode::SCCI64L:
        case Opcode::CONVERT:
        case Opcode::BFXS:
        case Opcode::BFXZ:
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
        case Opcode::SBINIMM64W: return true;
        default:                 return false;
    }
}

void DecodeAndAdvance(RT::Opcode opcode, Decoder::ByteReader& reader)
{
#define DECODE_INSTRUCTION(opcodeName, encoding, stringFormat)                                                         \
    case RT::Opcode::opcodeName: static_cast<void>(RT::encoding::Decode(reader)); return;

    switch (opcode) {
        CBC_RT_OPCODES(DECODE_INSTRUCTION)
        default: FATAL("Unknown runtime opcode while checking inline eligibility");
    }

#undef DECODE_INSTRUCTION
}

bool HasOnlySupportedInstructions(const Interpretation::Code& code)
{
    auto* const end = code.bytecode + code.bytecodeSize;
    Decoder::ByteReader reader(code.bytecode, code.bytecode, end);
    while (!reader.EndOfMem(end)) {
        const auto opcode = RT::Opcode(reader.PeekOpcode());
        if (!IsSupportedInstruction(opcode)) {
            return false;
        }
        DecodeAndAdvance(opcode, reader);
    }
    return true;
}

InlineEligibilityResult Reject(InlineRejectionReason reason) { return { reason }; }

} // namespace

InlineEligibilityResult CheckInlineEligibility(
    Engine::Session& session,
    Interpretation::DynamicFunctionHandle* callerFuh,
    Interpretation::DynamicFunctionHandle* calleeFuh,
    Interpretation::ExecBytecodeInfo* expectedCalleeBytecode
)
{
    if (callerFuh == calleeFuh) {
        return Reject(InlineRejectionReason::RECURSION);
    }

    if (expectedCalleeBytecode == nullptr) {
        return Reject(InlineRejectionReason::MISSING_BYTECODE);
    }

    const auto definition = Decode::Read(session, calleeFuh->methodDef);
    const auto codeId     = definition.MethodCode();
    if (!codeId.has_value()) {
        return Reject(InlineRejectionReason::MISSING_BYTECODE);
    }
    auto code = Decode::Read(session, *codeId);

    if (code.CodeSize() > MAX_INLINE_CBC_SIZE) {
        return Reject(InlineRejectionReason::TOO_LARGE);
    }
    if (code.UntypedSlotCount() != 0 || code.StackAllocSigsCount() != 0) {
        return Reject(InlineRejectionReason::STACK_SLOTS);
    }
    if (code.rawExTable.start != code.rawExTable.end) {
        return Reject(InlineRejectionReason::EXCEPTION_REGIONS);
    }
    if (code.rawLivenessInfo.start != code.rawLivenessInfo.end) {
        return Reject(InlineRejectionReason::LIVENESS_INFO);
    }
    if (code.rawStackPtrsInfo.start != code.rawStackPtrsInfo.end) {
        return Reject(InlineRejectionReason::STACK_PTRS_INFO);
    }

    const auto abiFlags = definition.GetABIFlags();
    const auto& abi     = expectedCalleeBytecode->abiInfo;
    if (abi.isSRet || abi.hasTailReg || abi.stackPtrParams != 0 || abi.derivedPairs != 0 ||
        abiFlags.Is(Image::MethodRefFlag::HAS_THIS_TI) || abiFlags.Is(Image::MethodRefFlag::HAS_OUTER_TI) ||
        abiFlags.Is(Image::MethodRefFlag::HAS_FTVARS)) {
        return Reject(InlineRejectionReason::UNSUPPORTED_ABI);
    }
    if (expectedCalleeBytecode->savedIRegs.value != 0 || expectedCalleeBytecode->savedFRegs.value != 0) {
        return Reject(InlineRejectionReason::NON_VOLATILE_REGISTERS);
    }
    if (!HasOnlySupportedInstructions(expectedCalleeBytecode->code)) {
        return Reject(InlineRejectionReason::UNSUPPORTED_INSTRUCTION);
    }
    return { InlineRejectionReason::NONE };
}

Stream::Output& operator<<(Stream::Output& out, InlineRejectionReason reason)
{
    switch (reason) {
        case InlineRejectionReason::NONE:                    return out << "none";
        case InlineRejectionReason::RECURSION:               return out << "recursion";
        case InlineRejectionReason::MISSING_BYTECODE:        return out << "missing bytecode";
        case InlineRejectionReason::TOO_LARGE:               return out << "callee is too large";
        case InlineRejectionReason::STACK_SLOTS:             return out << "callee uses stack slots";
        case InlineRejectionReason::LIVENESS_INFO:           return out << "callee has liveness info";
        case InlineRejectionReason::STACK_PTRS_INFO:         return out << "callee has stack pointers info";
        case InlineRejectionReason::EXCEPTION_REGIONS:       return out << "callee has exception regions";
        case InlineRejectionReason::UNSUPPORTED_ABI:         return out << "callee has unsupported ABI";
        case InlineRejectionReason::NON_VOLATILE_REGISTERS:  return out << "callee uses non-volatile registers";
        case InlineRejectionReason::UNSUPPORTED_INSTRUCTION: return out << "callee contains an unsupported instruction";
    }
    return out << "unknown";
}

} // namespace Cbc
