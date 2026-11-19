#include "inline_eligibility.h"

#include "engine/decode/reader.h"
#include "engine/image/flags.h"
#include "inlining/runtime_bytecode.h"
#include "interpreter/function_handle.h"
#include "utils/ostream.h"

namespace Cbc {
namespace {

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
    if (code.UntypedSlotCount() != 0 || code.StackAllocSigsCount() != 0 ||
        expectedCalleeBytecode->frameSize != 0 || expectedCalleeBytecode->staticFrameSize != 0) {
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
        definition->abiKind != Image::MethodAbiKind::STATIC || abiFlags.Is(Image::MethodRefFlag::HAS_OUTER_TI) ||
        abiFlags.Is(Image::MethodRefFlag::HAS_FTVARS)) {
        return Reject(InlineRejectionReason::UNSUPPORTED_ABI);
    }
    if (expectedCalleeBytecode->savedIRegs.value != 0 || expectedCalleeBytecode->savedFRegs.value != 0) {
        return Reject(InlineRejectionReason::NON_VOLATILE_REGISTERS);
    }
    if (!expectedCalleeBytecode->gcInfo.positionalInfo.Empty() ||
        !expectedCalleeBytecode->stackPtrsInfo.positionalInfo.Empty() ||
        !IsSupportedBody(expectedCalleeBytecode->code)) {
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
        case InlineRejectionReason::STACK_SLOTS:             return out << "callee or caller uses stack slots";
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
