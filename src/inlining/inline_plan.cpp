#include "inline_plan.h"

#include "utils/ostream.h"

namespace Cbc {

Stream::Output& operator<<(Stream::Output& out, const InlinePlan& plan)
{
    out.Print(
        "InlinePlan { caller: {}, bytecode: {}, version: {} -> {}, callSites: {} }",
        static_cast<const void*>(plan.callerFuh),
        static_cast<const void*>(plan.expectedBytecode),
        plan.expectedVersion,
        plan.expectedVersion + 1,
        plan.callSites.size()
    );
    return out;
}

} // namespace Cbc
