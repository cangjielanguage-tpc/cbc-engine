#pragma once

#include "utils/options.h"

namespace Engine {

using Options::Table;

extern bool useShortGCTib;

void InitEnvOptions();
extern Table const g_table;

} // namespace Engine
