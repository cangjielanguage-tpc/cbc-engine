#include "engine/options.h"

#include "cbc/isa_disasm.h"
#include "interpreter/loggers.h"
#include "resolution/resolution.h"
#include "runtimesupport/impl/entrypoint.h"

#include "utils/logger.h"

namespace Engine {

using Options::Option;
using Options::SetLogLevelValue;
using Options::SetAllLogLevels;
using Options::SetBoolValue;
using Options::SetStringValue;
using Options::Table;

constexpr Option globalOptionsArray[] = {
    { "cbc.log.resolution", &Resolution::log, &SetLogLevelValue },
    { "cbc.log.int", &Interpretation::Log::interpretation, &SetLogLevelValue },
    { "cbc.log.preparation", &Interpretation::Log::preparation, &SetLogLevelValue },
    { "cbc.log.all", nullptr, &SetAllLogLevels },
    { "cbc.dasm", &Cbc::g_IsRawDisasmEnabled, &SetBoolValue },
    { "cbc.path", &g_cbcPath, &SetStringValue },
    { "cbc.main", &g_mainCbc, &SetStringValue },
};

Table const g_table(globalOptionsArray);

void InitEnvOptions()
{
    Options::InitFromEnv(g_table);
}

} // namespace Engine
