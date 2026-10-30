#pragma once

#include "profiler/fiber_profile.h"
#include "utils/logger.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Cbc::Profiler {

struct ProfiledCall {
    FunctionId caller;
    std::string callerName;
    uintptr_t callSiteOffset;
    FunctionId callee;
    std::string calleeName;
    uint64_t count;
    bool hot;
};

extern bool enabled;

namespace Log {
extern Stream::Descripted stream;
extern Logging::Logger profiler;
} // namespace Log

FiberProfileHandle CreateFiberProfile();

// Call only after the last RecordCall on this fiber has completed.
void DestroyFiberProfile(FiberProfileHandle& profile);

} // namespace Cbc::Profiler
