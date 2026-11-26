#include "inline_eligibility.h"

#include "engine/decode/reader.h"
#include "inlining/runtime_bytecode.h"
#include "inlining/runtime_frame.h"
#include "interpreter/function_handle.h"
#include "utils/ostream.h"

namespace Cbc {

InlineRejectionReason CheckInlineEligibility(
    Engine::Session& session,
    Interpretation::DynamicFunctionHandle* callerFuh,
    Interpretation::DynamicFunctionHandle* calleeFuh,
    Interpretation::ExecBytecodeInfo* expectedCalleeBytecode
)
{
    if (callerFuh == calleeFuh) {
        return InlineRejectionReason::RECURSION;
    }

    if (expectedCalleeBytecode == nullptr) {
        return InlineRejectionReason::MISSING_BYTECODE;
    }

    const auto definition = Decode::Read(session, calleeFuh->methodDef);
    const auto codeId     = definition.MethodCode();
    if (!codeId.has_value()) {
        return InlineRejectionReason::MISSING_BYTECODE;
    }

    auto code = Decode::Read(session, *codeId);
    const auto& calleeCode = expectedCalleeBytecode->code;

    if (calleeCode.bytecodeSize > MAX_CODE_SIZE) {
        return InlineRejectionReason::TOO_LARGE;
    }
    if (!HasSupportedInlineFrame(*expectedCalleeBytecode)) {
        return InlineRejectionReason::STACK_SLOTS;
    }
    if (code.rawExTable.start != code.rawExTable.end) {
        return InlineRejectionReason::EXCEPTION_REGIONS;
    }
    if (code.rawLivenessInfo.start != code.rawLivenessInfo.end) {
        return InlineRejectionReason::LIVENESS_INFO;
    }
    if (code.rawStackPtrsInfo.start != code.rawStackPtrsInfo.end) {
        return InlineRejectionReason::STACK_PTRS_INFO;
    }

    const auto& abi = expectedCalleeBytecode->abiInfo;
    if (abi.isSRet || abi.hasTailReg || abi.stackPtrParams != 0 || abi.derivedPairs != 0) {
        return InlineRejectionReason::UNSUPPORTED_ABI;
    }
    if (!expectedCalleeBytecode->gcInfo.positionalInfo.Empty() ||
        !expectedCalleeBytecode->stackPtrsInfo.positionalInfo.Empty()) {
        return InlineRejectionReason::UNSUPPORTED_INSTRUCTION;
    }
    if (!IsSupportedBody(calleeCode, DecodeRuntimeBytecode(calleeCode))) {
        return InlineRejectionReason::UNSUPPORTED_INSTRUCTION;
    }
    return InlineRejectionReason::NONE;
}

Stream::Output& operator<<(Stream::Output& out, InlineRejectionReason reason)
{
    switch (reason) {
        case InlineRejectionReason::NONE:                    return out << "none";
        case InlineRejectionReason::RECURSION:               return out << "recursion";
        case InlineRejectionReason::MISSING_BYTECODE:        return out << "missing bytecode";
        case InlineRejectionReason::TOO_LARGE:               return out << "callee is too large";
        case InlineRejectionReason::STACK_SLOTS:             return out << "callee has an unsupported frame layout";
        case InlineRejectionReason::LIVENESS_INFO:           return out << "callee has liveness info";
        case InlineRejectionReason::STACK_PTRS_INFO:         return out << "callee has stack pointers info";
        case InlineRejectionReason::EXCEPTION_REGIONS:       return out << "callee has exception regions";
        case InlineRejectionReason::UNSUPPORTED_ABI:         return out << "callee has unsupported ABI";
        case InlineRejectionReason::UNSUPPORTED_INSTRUCTION: return out << "callee contains an unsupported instruction";
    }
    return out << "unknown";
}

} // namespace Cbc
