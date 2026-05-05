#include <gtest/gtest.h>

#include "cbc/isa_disasm.h"
#include "resolution/resolution.h"
#include "runtimesupport/impl/entrypoint.h"

#include "utils/logger.h"
#include "utils/options.h"

TEST(Options, fromCStr)
{
    const char* options[] = {
        "cbc.log.resolution=trace", "cbc.log.int=info", "cbc.dasm=true", "cbc.path=/path/to/cbc/sources"
    };
    constexpr size_t optionsCount = sizeof(options) / sizeof(decltype(options[0]));

    Options::ParseAndSetOptions(optionsCount, options);

    ASSERT(Resolution::log.GetLogLevel() == Logging::Level::TRACE);
    ASSERT(Cbc::g_IsRawDisasmEnabled);
    ASSERT(!g_cbcPath.compare("/path/to/cbc/sources"));
}
