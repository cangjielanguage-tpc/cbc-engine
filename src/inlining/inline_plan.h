#pragma once

#include <cstdint>
#include <vector>

namespace Stream {
class Output;
}

namespace Interpretation {
struct DynamicFunctionHandle;
struct ExecBytecodeInfo;
} // namespace Interpretation

namespace Cbc {

struct InlineCallSite {
    Interpretation::DynamicFunctionHandle* calleeFuh;
    Interpretation::ExecBytecodeInfo* expectedCalleeBytecode;
    uint64_t expectedCalleeVersion;
    uint32_t cbcCallSiteOffset;
    uint32_t runtimeCallSiteOffset;
};

struct InlinePlan {
    Interpretation::DynamicFunctionHandle* callerFuh;
    Interpretation::ExecBytecodeInfo* expectedBytecode;
    uint64_t expectedVersion;
    std::vector<InlineCallSite> callSites;
};

Stream::Output& operator<<(Stream::Output& out, const InlinePlan& plan);

} // namespace Cbc
