#include "utils/options.h"

#include "cbc/isa_disasm.h"
#include "interpreter/loggers.h"
#include "resolution/resolution.h"
#include "runtimesupport/impl/entrypoint.h"

#include "utils/logger.h"

namespace Options {

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

} // namespace Options
