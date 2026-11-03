#pragma once

#include "profiler/profiler.h"

namespace Cbc::Profiler {

void LogProfilerPass(const std::vector<ProfiledCall>& calls, uint64_t recordedCallCount, uint64_t scanTimeUs);

} // namespace Cbc::Profiler
