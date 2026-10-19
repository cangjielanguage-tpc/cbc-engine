#pragma once

#include "profiler/profiler.h"

namespace Cbc::Profiler {

void LogProfilerPass(const std::vector<ProfiledCall>& calls);

} // namespace Cbc::Profiler
