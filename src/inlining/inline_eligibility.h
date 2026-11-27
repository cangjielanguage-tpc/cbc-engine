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

constexpr uint32_t MAX_CODE_SIZE = 8 * 1024;

enum class InlineRejectionReason {
    NONE,
    RECURSION,
    MISSING_BYTECODE,
    TOO_LARGE,
    STACK_SLOTS,
    LIVENESS_INFO,
    EXCEPTION_REGIONS,
    UNSUPPORTED_ABI,
    UNSUPPORTED_INSTRUCTION,
};

InlineRejectionReason CheckInlineEligibility(
    Engine::Session& session,
    Interpretation::DynamicFunctionHandle* callerFuh,
    Interpretation::DynamicFunctionHandle* calleeFuh,
    Interpretation::ExecBytecodeInfo* expectedCalleeBytecode
);

Stream::Output& operator<<(Stream::Output& out, InlineRejectionReason reason);

} // namespace Cbc
