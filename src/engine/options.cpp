#include "engine/options.h"

#include "cbc/isa_disasm.h"
#include "engine/field_layout.h"
#include "engine/method_table.h"
#include "interpreter/loggers.h"
#include "resolution/resolution.h"
#include "runtimesupport/impl/entrypoint.h"

#include "utils/options.h"
#include "utils/rt_logger.h"

using Options::Option;
using Options::SetAllLogLevels;
using Options::SetBoolValue;
using Options::SetIntValue;
using Options::SetLogLevelValue;
using Options::SetStringValue;
using Options::Table;

bool Engine::useShortGCTib = true;

constexpr Option globalOptionsArray[] = {
    { "cbc.use.short.gctib", &Engine::useShortGCTib, &SetBoolValue },
    { "cbc.log.resolution", &Resolution::log, &SetLogLevelValue },
    { "cbc.log.int", &Interpretation::Log::interpretation, &SetLogLevelValue },
    { "cbc.log.preparation", &Interpretation::Log::preparation, &SetLogLevelValue },
    { "cbc.log.method.table", &Engine::Log::mt, &SetLogLevelValue },
    { "cbc.log.field.layout", &Engine::Log::fields, &SetLogLevelValue },
    { "cbc.log.root.scan", &RTSupport::Log::gc, &SetLogLevelValue },
    { "cbc.log.runtime", &RTSupport::Log::rt, &SetLogLevelValue },
    { "cbc.log.init", &RTSupport::Log::init, &SetLogLevelValue },
    { "cbc.log.typeinfo", &RTSupport::Log::typeinfo, &SetLogLevelValue },
    { "cbc.log.all", nullptr, &SetAllLogLevels },
    { "cbc.dasm", &Cbc::g_IsRawDisasmEnabled, &SetBoolValue },
    { "cbc.path", &g_cbcPath, &SetStringValue },
    { "cbc.main", &g_mainCbc, &SetStringValue },
    { "cbc.skip.n.int", &Interpretation::Log::skipThreshold, &SetIntValue },
};

namespace Engine {

Table const g_table(globalOptionsArray);

void InitEnvOptions()
{
    Options::InitFromEnv(g_table);
}

} // namespace Engine
