#include <dlfcn.h>

#include <cstdio>
#include <iterator>
#include <utility>

#include "cbc_engine.h"
#include "utils/logger.h"
#include "utils/rt_logger.h"

namespace {

static void* FindSymbolInLibrary(void* ctx, char const* name, char buffer[CBC_FIND_SYM_ERR_MAX_LEN])
{
    dlerror(); // clear any error from an earlier loader operation
    void* symbol = dlsym(ctx, name);
    if (symbol == nullptr) {
        auto error = dlerror();
        std::snprintf(buffer, CBC_FIND_SYM_ERR_MAX_LEN, "%s", error != nullptr ? error : "symbol not found");
        return nullptr;
    }
    return symbol;
}

static CbcStatus MakeLibraryResolver(CbcDlsym* rloc, void* handle)
{
    CbcDlsym resolver;
    resolver.ctx  = handle;
    resolver.find = &FindSymbolInLibrary;
    *rloc         = resolver;
    return CBC_OK;
}

} // namespace

extern "C" {

CBC_EXPORT CbcStatus engine_preset_cjnative_builtins(CbcDlsym resolver)
{
    if (resolver.find == nullptr) {
        return CBC_INVALID_ARGUMENT;
    }

    char buffer[CBC_FIND_SYM_ERR_MAX_LEN];
    auto find = [&](char const* name) -> void* {
        void* sym = resolver.find(resolver.ctx, name, buffer);
        if (sym == nullptr) {
            LOG_ERROR(RTSupport::Log::rt, "builtin symbol {} not found: {}", name, buffer);
        }
        return sym;
    };

    auto futureExecute      = find("_CNat6FutureIG_E7executeHv");
    auto threadHandleSetter = find("_CNat6Thread24setRuntimeCJThreadHandleHPu");

    CbcFunction thrower{};
    auto status = engine_query_function(&thrower, "$P$cangjie.runtime.cbcengine.helper", "_CN32cangjie.runtime.cbcengine.helper22throwImplicitExceptionHl", true);

    if (status != CBC_OK || futureExecute == nullptr || threadHandleSetter == nullptr) {
        return CBC_SYMBOL_NOT_FOUND;
    }

    CjBuiltins builtins {};
    builtins.implicitThrower.code           = thrower.code;
    builtins.futureExecute.code             = futureExecute;
    builtins.setRuntimeCJThreadHandle.code  = threadHandleSetter;
    return engine_lib_initialize_builtins(&builtins);
}

CBC_EXPORT CbcStatus engine_preset_cbc_builtins(void)
{
    CjBuiltins builtins {};
    struct BuiltinQuery {
        char const* typeName;
        char const* funcName;
        void** slot;
        char const* label;
    };
    const BuiltinQuery queries[] = {
        {"$P$cangjie.runtime.cbcengine.helper", "throwImplicitException", &builtins.implicitThrower.code, "implicit exception thrower"},
        {"std.core:Future", "execute", &builtins.futureExecute.code, "Future.execute"},
        {"std.core:Thread", "setRuntimeCJThreadHandle", &builtins.setRuntimeCJThreadHandle.code, "Thread.setRuntimeCJThreadHandle"},
    };

    for (auto const& query : queries) {
        CbcFunction fn {};
        CbcStatus status = engine_query_function(&fn, query.typeName, query.funcName, 1);
        if (status == CBC_RESOLUTION_ERROR) {
            // The builtin is not provided by the loaded CBC images; it stays
            // unavailable and the operations that require it cannot be executed.
            LOG_ERROR(RTSupport::Log::rt, "cbc builtin {} not found in cbc images: {}:{}", query.label, query.typeName, query.funcName);
            continue;
        }
        if (status != CBC_OK) {
            return status;
        }
        *query.slot = fn.code;
    }

    return engine_lib_initialize_builtins(&builtins);
}

CBC_EXPORT CbcStatus engine_preset_init_staging(CbcDlsym resolver)
{
    if (resolver.find == nullptr) {
        return CBC_INVALID_ARGUMENT;
    }

    // Staging tokens are stable engine-side identifiers; a plain symbol lookup
    // resolver only knows the actual runtime symbol names, so fall back to them.
    static constexpr std::pair<char const*, char const*> stagingSymbolNames[] = {
        { READ_WEAK_REF_TOKEN, "CJ_MCC_ReadWeakRef" },
    };

    char buffer[CBC_FIND_SYM_ERR_MAX_LEN];
    CbcStagingSym symbols[std::size(stagingSymbolNames)];
    int count = 0;

    for (auto const& stagingSymbol : stagingSymbolNames) {
        char const* token      = stagingSymbol.first;
        char const* symbolName = stagingSymbol.second;
        void* symbol = resolver.find(resolver.ctx, token, buffer);
        if (symbol == nullptr) {
            symbol = resolver.find(resolver.ctx, symbolName, buffer);
        }
        if (symbol == nullptr) {
            LOG_ERROR(RTSupport::Log::rt, "staging symbol {} not found: {}", token, buffer);
            return CBC_SYMBOL_NOT_FOUND;
        }
        symbols[count++] = { token, symbol };
    }

    return engine_initialize_rt_staging(symbols, count);
}

CBC_EXPORT CbcStatus engine_preset_in_executable_resolver(CbcDlsym* rloc)
{
    if (rloc == nullptr) {
        return CBC_INVALID_ARGUMENT;
    }
    void* handle = dlopen(nullptr, RTLD_LAZY);
    if (handle == nullptr) {
        LOG_ERROR(RTSupport::Log::rt, "failed to open the current executable: {}", dlerror());
        return CBC_PLATFORM_ERROR;
    }
    // The handle is intentionally never closed: the resolver may be used at any moment
    // of the process lifetime.
    return MakeLibraryResolver(rloc, handle);
}

CBC_EXPORT CbcStatus engine_preset_in_library_resolver(CbcDlsym* rloc, void* handle)
{
    if (rloc == nullptr || handle == nullptr) {
        return CBC_INVALID_ARGUMENT;
    }
    return MakeLibraryResolver(rloc, handle);
}

CBC_EXPORT CbcStatus engine_preset_in_host_library_resolver(CbcDlsym* rloc, void* symbolLocation)
{
    if (rloc == nullptr || symbolLocation == nullptr) {
        return CBC_INVALID_ARGUMENT;
    }
    Dl_info info {};
    if (dladdr(symbolLocation, &info) == 0 || info.dli_fname == nullptr) {
        LOG_ERROR(RTSupport::Log::rt, "dladdr failed to find the host library of {}", symbolLocation);
        return CBC_PLATFORM_ERROR;
    }
    void* handle = dlopen(info.dli_fname, RTLD_LAZY | RTLD_LOCAL);
    if (handle == nullptr) {
        LOG_ERROR(RTSupport::Log::rt, "failed to open the host library {}: {}", info.dli_fname, dlerror());
        return CBC_PLATFORM_ERROR;
    }
    // The handle is intentionally never closed: the resolver may be used at any moment
    // of the process lifetime.
    return MakeLibraryResolver(rloc, handle);
}

} // extern "C"
