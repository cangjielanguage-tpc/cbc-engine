#pragma once

#include <cstdint>

namespace Stream {
class Output;
}

namespace Engine {
class Session;
}

namespace Interpretation {
struct DynamicFunctionHandle;
struct ExecBytecodeInfo;
}

namespace Cbc {

constexpr uint32_t MAX_INLINE_CBC_SIZE = 128;

enum class InlineRejectionReason {
    NONE,
    RECURSION,
    MISSING_BYTECODE,
    TOO_LARGE,
    STACK_SLOTS,
    LIVENESS_INFO,
    STACK_PTRS_INFO,
    EXCEPTION_REGIONS,
    UNSUPPORTED_ABI,
    NON_VOLATILE_REGISTERS,
    UNSUPPORTED_INSTRUCTION,
};

struct InlineEligibilityResult {
    InlineRejectionReason reason;

    bool IsAllowed() const { return reason == InlineRejectionReason::NONE; }
};

InlineEligibilityResult CheckInlineEligibility(
    Engine::Session& session,
    Interpretation::DynamicFunctionHandle* callerFuh,
    Interpretation::DynamicFunctionHandle* calleeFuh,
    Interpretation::ExecBytecodeInfo* expectedCalleeBytecode
);

Stream::Output& operator<<(Stream::Output& out, InlineRejectionReason reason);

} // namespace Cbc
