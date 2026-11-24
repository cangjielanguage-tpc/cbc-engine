#include "runtimesupport/impl/entrypoint.h"

#include <dlfcn.h>

#include <cstring>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "RTInterface.h"
#include "asm_export.h"
#include "asm_trampolines.h"
#include "cbc_engine.h"
#include "cbc_loader.h"
#include "cjnative.h"
#include "engine/engine.h"
#include "engine/image/io/filesystem.h"
#include "engine/image/reader.h"
#include "engine/options.h"
#include "engine/resolving_output.h"
#include "interpreter/ectype.h"
#include "interpreter/function_handle.h"
#include "rt_interface.h"
#include "runtimesupport/adapters.h"
#include "runtimesupport/impl/rt_syms.h"
#include "runtimesupport/typeinfo_factory.h"
#include "utils/assertion.h"
#include "utils/logger.h"
#include "utils/ostream.h"
#include "utils/rt_logger.h"

DYN_CJNativeInterface g_CJNativeInterfaceInstance;

static std::mutex g_InitializationGuard;
static bool g_Loaded;
static bool g_BridgeInitialized;
static bool g_Patched;

static CbcDlsym g_Resolver {};
static bool g_HasResolver;

bool IsEngineLoaded()
{
    return g_Loaded;
}

static std::string EngineLibraryDirectory()
{
    Dl_info info {};
    if (dladdr(reinterpret_cast<void*>(&EngineLibraryDirectory), &info) == 0 || info.dli_fname == nullptr) {
        return {};
    }
    std::string_view path(info.dli_fname);
    // TODO: make search of dir fs helper if we have them/will have them
    auto separator = path.find_last_of('/');
    if (separator == std::string_view::npos) {
        return {};
    }
    return std::string(path.substr(0, separator));
}

static void LoadCbcPaths(Engine::Loader& loader, std::string const& excludedCbc)
{
    std::string_view rest(g_cbcPath);
    while (!rest.empty()) {
        auto pos = rest.find(':');
        auto dir = rest.substr(0, pos);
        if (!dir.empty()) {
            RTSupport::LoadCbcFilesFromDirectory(loader, std::string(dir), excludedCbc);
        }
        if (pos == std::string_view::npos) {
            break;
        }
        rest.remove_prefix(pos + 1);
    }
}

/// Load the provided CBC inputs without executing code or querying the runtime.
static CbcStatus LoadEngine()
{
    std::lock_guard guard(g_InitializationGuard);
    if (g_Loaded) {
        return CBC_OK;
    }

    RTSupport::Log::rt.Log(Logging::Level::INFO, [](Stream::Output& out) {
        out << "start engine init" << Stream::endl;
    });

    if (!g_HasResolver) {
        LOG_ERROR(RTSupport::Log::rt, "no symbol resolver set: foreign functions and AOT symbols will not resolve");
    }

    Engine::Loader loader(g_Resolver.ctx, g_Resolver.find);

    if (!g_cbcPath.empty()) {
        LoadCbcPaths(loader, g_mainCbc);
    }

    if (!g_mainCbc.empty()) {
        auto file = IO::TryOpenFile(g_mainCbc);
        if (file.has_value()) {
            loader.Load(std::move(file.value()), g_mainCbc);
        } else {
            RTSupport::Log::rt.Log(Logging::Level::ERROR, [](Stream::Output& out) {
                out.PrintFmtLn("engine load: no such file or directory %s", g_mainCbc.c_str());
            });
            return CBC_FILE_NOT_FOUND;
        }
    }

    constexpr auto intrinsicsFileName = "/cbc-intrinsics.cbc";
    auto intrinsicsDir                = EngineLibraryDirectory();
    if (!intrinsicsDir.empty()) {
        auto intrinsicsPath = intrinsicsDir + std::string(intrinsicsFileName);
        if (auto file = IO::TryOpenFile(intrinsicsPath)) {
            loader.Load(std::move(*file), intrinsicsPath);
        } else {
            FATAL("engine load: cannot find path to intrinsics. given path: %s", intrinsicsPath.c_str());
        }
    }

    loader.Build();

    RTSupport::Log::rt.Log(Logging::Level::INFO, [](Stream::Output& out) { out << "end engine init" << Stream::endl; });

    g_Loaded = true;
    return CBC_OK;
}

static void EnsureHelpersPrepared(Engine::Session& session)
{
    auto& fuhManager = Interpretation::FunctionHandleManager::Of(session);
    fuhManager.FindAndPrepareHelpers(session);
}

static void PerformPatching()
{
    LOG_INFO(RTSupport::Log::rt, "start patching");

    auto& engine = Engine::GetEngineInstance();

    Engine::Session session(engine);

    auto& fuhManager = Interpretation::FunctionHandleManager::Of(engine);

    fuhManager.FindAndPrepareHelpers(session);

    for (auto& file : engine.Files()) {
        // TODO: list patches in CBC file header
        for (auto type : Decode::AllEntries(session, file.GetTypeIndex())) {
            auto def = Decode::Read(session, type);
            if (!def.GetFlags().Is(Image::TypeFlag::PATCH)) {
                continue;
            }

            auto pkgName = Decode::Read(session, def.GetName());
            pkgName      = pkgName.substr(3);

            LOG_INFO(RTSupport::Log::rt, "patching package {}", pkgName);

            auto patchPrefix = "$" + std::string(pkgName);

            auto patchFlagName  = patchPrefix + "$packageInit$GVF";
            auto patchClassName = std::string(pkgName) + ":" + patchPrefix + "$PackageInitPatch$GC";

            // Get patched type info
            auto ti = g_CJNativeInterfaceInstance.typeInfo(patchClassName.c_str());
            if (ti == nullptr) {
                LOG_ERROR(RTSupport::Log::rt, "patch TI not found {}", patchClassName);
                continue;
            }

            LOG_INFO(RTSupport::Log::rt, "patching ti found {}", patchClassName);

            // Corresponding extension def (TODO: check it)
            auto edef = ti->vExtensionDataStart[1];

            for (auto mdefId : Decode::AllEntries(session, def.GetMethods())) {
                auto mdef = Decode::Read(session, mdefId);

                auto idx = -1;
                if (mdef.GetFlags().Is(Image::MethodFlag::PKG_INIT)) {
                    idx = 0;
                }
                if (mdef.GetFlags().Is(Image::MethodFlag::LIT_INIT)) {
                    idx = 1;
                }

                if (idx != -1) {
                    LOGS_INFO(RTSupport::Log::rt, session, "patching funcTable[{}] with {}", idx, mdef.Name());
                    auto fuh = fuhManager.AcquireTagged(session, mdef.GetIdentifier());
                    auto ptr = fuhManager.GetFunctionPtrForDirectCall(fuh);

                    edef->funcTable[idx] = ptr;
                }
            };

            // Set patched flag
            auto& deps = engine.Dependencies().At(file.Id());
            auto flag  = deps.FindSymbol(patchFlagName);
            if (flag == nullptr) {
                LOG_ERROR(RTSupport::Log::rt, "patching flag field not found {}", patchFlagName);
                continue;
            }

            *(bool*)flag = true;
        };
    }

    RTSupport::Log::rt.Log(Logging::Level::INFO, [](Stream::Output& out) { out << "end patching" << Stream::endl; });
}

extern "C" Interpretation::Ectype* engine_fiber_data_init(DYN_CJThreadSpecificData* data)
{
    ASSERTION(*data == nullptr, "Incorrect data value: %p", *data);

    auto ectype = new Interpretation::Ectype();
    if (!ectype) {
        FATAL("Ectype allocation error");
    }

    *data = ectype;

    LOG_INFO(RTSupport::Log::rt, "fiber init, fsd: {}, ectype: {}", Stream::Hex(data), Stream::Hex(ectype));

    return ectype;
}

namespace {

CbcStatus SetTableOption(std::string_view key, std::string_view value)
{
    switch (Engine::g_table.Set(key, value)) {
        case Options::Table::Status::OK:
            return CBC_OK;
        case Options::Table::Status::INVALID_OPTION:
            LOG_ERROR(RTSupport::Log::rt, "invalid option {}={}", key, value);
            return CBC_INVALID_ARGUMENT;
        case Options::Table::Status::UNKNOWN_OPTION:
            LOG_ERROR(RTSupport::Log::rt, "unknown option {}={}", key, value);
            return CBC_INVALID_ARGUMENT;
    }
    return CBC_INVALID_ARGUMENT;
}

/// Splits a `key=value` token; returns false if the token is malformed.
bool SplitKeyVal(std::string_view token, std::string_view& key, std::string_view& value)
{
    auto eqPos = token.find('=');
    if (eqPos == std::string_view::npos || eqPos + 1 >= token.size()) {
        return false;
    }
    key   = token.substr(0, eqPos);
    value = token.substr(eqPos + 1);
    return true;
}

CbcStatus FillQueryResult(
    CbcFunction* rloc,
    Engine::Session& session,
    Image::Identifier<Image::MethodDefinition> methodDef,
    bool needTrampoline
)
{
    auto& engine     = Engine::GetEngineInstance();
    auto& fuhManager = Interpretation::FunctionHandleManager::Of(engine);

    auto tagged = fuhManager.AcquireTagged(session, methodDef);

    rloc->code = nullptr;
    rloc->fuh  = nullptr;

    if (auto* staticFuh = std::get_if<Interpretation::StaticFunctionHandle*>(&tagged)) {
        // AOT compiled function is directly callable, no trampoline is needed.
        rloc->code = (*staticFuh)->function;
        rloc->fuh  = reinterpret_cast<struct FunctionHandle*>(&(*staticFuh)->base);
        return CBC_OK;
    }

    auto* dynFuh = std::get<Interpretation::DynamicFunctionHandle*>(tagged);
    rloc->fuh    = reinterpret_cast<struct FunctionHandle*>(&dynFuh->base);

    if (needTrampoline) {
        auto trampoline = RTSupport::Adapters::TryGetDirectCallTrampoline(dynFuh);
        if (trampoline == nullptr) {
            LOG_ERROR(RTSupport::Log::rt, "trampoline pool exhausted for query {}", methodDef.Pack());
            return CBC_TRAMPOLINES_EXHAUSTED;
        }
        rloc->code = trampoline;
    }
    return CBC_OK;
}

/// Finds a method by the "<pkg name>:<func name>" query.
/// The enclosing type is either the `<pkg name>` prefix or the whole query
/// (some layouts store package level functions in a type named "<pkg>:<func>").
std::optional<Image::Identifier<Image::MethodDefinition>> FindMethodByName(
    Engine::Session& session, std::string_view name, CbcStatus* status
)
{
    auto& engine = Engine::GetEngineInstance();

    auto colon = name.find(':');
    if (colon == std::string_view::npos || colon == 0 || colon + 1 == name.size()) {
        LOG_ERROR(RTSupport::Log::rt, "malformed function query {}, expected \"<pkg name>:<func name>\"", name);
        *status = CBC_INVALID_ARGUMENT;
        return std::nullopt;
    }

    auto methodName = name.substr(colon + 1);

    std::string_view typeNames[] = { name.substr(0, colon), name };

    for (auto typeName : typeNames) {
        auto type = engine.FindType(session, typeName);
        if (!type.has_value()) {
            continue;
        }

        auto typeDef = Decode::Read(session, type.value());

        int count                                                        = 0;
        std::optional<Image::Identifier<Image::MethodDefinition>> found  = std::nullopt;
        for (auto m : Decode::Reader::FindBucket(session, typeDef.GetMethods(), methodName)) {
            count++;
            found = m;
        }

        if (count > 1) {
            LOG_ERROR(RTSupport::Log::rt, "ambiguous function query {}", name);
            *status = CBC_AMBIGUOUS_RESOLUTION_ERROR;
            return std::nullopt;
        }
        if (count == 1) {
            *status = CBC_OK;
            return found;
        }
    }

    LOG_ERROR(RTSupport::Log::rt, "function not found {}", name);
    *status = CBC_RESOLUTION_ERROR;
    return std::nullopt;
}

} // namespace

extern "C" {
/// This symbol is exported to the runtime, which would initialize engine.
CBC_EXPORT int interpreter_bridge_init(
    struct INT_InterpreterInterface* interpInterf,
    struct DYN_CJNativeInterface* rtInterf,
    int size,
    const char** options
);

CBC_EXPORT CbcStatus engine_set_cbcpath(char const* cbcPath)
{
    if (cbcPath == nullptr || cbcPath[0] == '\0') {
        return CBC_INVALID_ARGUMENT;
    }
    std::lock_guard guard(g_InitializationGuard);
    if (g_Loaded) {
        return CBC_INVALID_STATE;
    }
    if (g_cbcPath.empty()) {
        g_cbcPath = cbcPath;
    } else {
        g_cbcPath += ':';
        g_cbcPath += cbcPath;
    }
    return CBC_OK;
}

CBC_EXPORT CbcStatus engine_set_main_cbc(char const* mainCbc)
{
    if (mainCbc == nullptr || mainCbc[0] == '\0') {
        return CBC_INVALID_ARGUMENT;
    }
    std::lock_guard guard(g_InitializationGuard);
    if (g_Loaded) {
        return CBC_INVALID_STATE;
    }
    g_mainCbc = mainCbc;
    return CBC_OK;
}

CBC_EXPORT CbcStatus engine_parse_options(char const* text)
{
    if (text == nullptr) {
        return CBC_OK;
    }
    std::lock_guard guard(g_InitializationGuard);

    std::string_view rest(text);
    while (!rest.empty()) {
        auto pos   = rest.find(' ');
        auto token = rest.substr(0, pos);

        if (!token.empty()) {
            std::string_view key;
            std::string_view value;
            if (!SplitKeyVal(token, key, value)) {
                LOG_ERROR(RTSupport::Log::rt, "invalid option format {}", token);
                return CBC_INVALID_ARGUMENT;
            }
            auto status = SetTableOption(key, value);
            if (status != CBC_OK) {
                return status;
            }
        }

        if (pos == std::string_view::npos) {
            break;
        }
        rest.remove_prefix(pos + 1);
    }
    return CBC_OK;
}

CBC_EXPORT CbcStatus engine_set_kv_option(char const* key, char const* value)
{
    if (key == nullptr || value == nullptr) {
        return CBC_INVALID_ARGUMENT;
    }
    std::lock_guard guard(g_InitializationGuard);
    return SetTableOption(key, value);
}

CBC_EXPORT CbcStatus engine_set_symbol_resolver(CbcDlsym dlsym)
{
    if (dlsym.find == nullptr) {
        return CBC_INVALID_ARGUMENT;
    }
    std::lock_guard guard(g_InitializationGuard);
    g_Resolver    = dlsym;
    g_HasResolver = true;
    return CBC_OK;
}

CBC_EXPORT CbcStatus engine_load() { return LoadEngine(); }

CBC_EXPORT CbcStatus engine_query_function(CbcFunction* rloc, char const* name, int needTrampoline)
{
    if (rloc == nullptr || name == nullptr) {
        return CBC_INVALID_ARGUMENT;
    }
    std::lock_guard guard(g_InitializationGuard);
    if (!g_Loaded) {
        return CBC_NOT_LOADED;
    }

    auto& engine = Engine::GetEngineInstance();
    Engine::Session session(engine);
    EnsureHelpersPrepared(session);

    CbcStatus status    = CBC_OK;
    auto method = FindMethodByName(session, name, &status);
    if (!method.has_value()) {
        return status;
    }
    return FillQueryResult(rloc, session, method.value(), needTrampoline != 0);
}

CBC_EXPORT CbcStatus engine_query_main_function(CbcFunction* rloc)
{
    if (rloc == nullptr) {
        return CBC_INVALID_ARGUMENT;
    }
    std::lock_guard guard(g_InitializationGuard);
    if (!g_Loaded) {
        return CBC_NOT_LOADED;
    }
    if (g_mainCbc.empty()) {
        LOG_ERROR(RTSupport::Log::rt, "main cbc is not set");
        return CBC_MAIN_NOT_SET;
    }

    auto& engine = Engine::GetEngineInstance();
    Engine::Session session(engine);
    EnsureHelpersPrepared(session);

    auto main = engine.FindMain(session, g_mainCbc);
    if (!main.has_value()) {
        LOG_ERROR(RTSupport::Log::rt, "main function not found in {}", g_mainCbc);
        return CBC_RESOLUTION_ERROR;
    }
    return FillQueryResult(rloc, session, main.value(), true);
}

CBC_EXPORT CbcStatus engine_initialize_rt_staging(CbcStagingSym* symbols, int count)
{
    if (count < 0 || (count > 0 && symbols == nullptr)) {
        return CBC_INVALID_ARGUMENT;
    }
    std::lock_guard guard(g_InitializationGuard);

    for (int i = 0; i < count; i++) {
        auto& sym = symbols[i];
        if (sym.token == nullptr) {
            return CBC_INVALID_ARGUMENT;
        }
        if (std::strcmp(sym.token, READ_WEAK_REF_TOKEN) == 0) {
            if (sym.symbol == nullptr) {
                return CBC_INVALID_ARGUMENT;
            }
            RTSupport::InitializeStaging(sym.symbol);
            continue;
        }
        LOG_ERROR(RTSupport::Log::rt, "unknown staging symbol token {}", sym.token);
        return CBC_UNKNOWN_TOKEN;
    }
    return CBC_OK;
}

CBC_EXPORT CbcStatus engine_lib_initialize_builtins(CjBuiltins* builtins)
{
    if (builtins == nullptr) {
        return CBC_INVALID_ARGUMENT;
    }
    // The builtins are invoked from hand-written assembly, so the `code` part is mandatory.
    if (builtins->implicitThrower.code == nullptr || builtins->futureExecute.code == nullptr ||
        builtins->setRuntimeCJThreadHandle.code == nullptr) {
        return CBC_INVALID_ARGUMENT;
    }
    std::lock_guard guard(g_InitializationGuard);
    RTSupport::InstallBuiltins(
        builtins->implicitThrower.code,
        builtins->futureExecute.code,
        builtins->setRuntimeCJThreadHandle.code
    );
    return CBC_OK;
}

CBC_EXPORT CbcStatus engine_apply_hotfix_patches()
{
    std::lock_guard guard(g_InitializationGuard);
    if (!g_Loaded) {
        return CBC_NOT_LOADED;
    }
    if (!g_BridgeInitialized) {
        LOG_ERROR(RTSupport::Log::rt, "hotfix patching requires the runtime to be initialized");
        return CBC_RUNTIME_NOT_INITIALIZED;
    }
    if (g_Patched) {
        return CBC_OK;
    }

    PerformPatching();
    g_Patched = true;
    return CBC_OK;
}

CBC_EXPORT int interpreter_bridge_init(
    struct INT_InterpreterInterface* interpInterf,
    struct DYN_CJNativeInterface* rtInterf,
    int size,
    const char** options
)
{
    (void)size;
    (void)options; // deprecated: options are set through the configuration stage functions
    static_assert(std::is_same_v<decltype(&interpreter_bridge_init), INT_InitInterpreter>);
    if (rtInterf == nullptr || rtInterf->version != DYN_CJNATIVE_INTERFACE_VERSION) {
        LOG_ERROR(
            RTSupport::Log::rt,
            "Unexpected DYN_CJNativeInterface version: expected {}, actual {}",
            DYN_CJNATIVE_INTERFACE_VERSION,
            rtInterf != nullptr ? rtInterf->version : -1
        );
        return 1;
    }

    if (!RTSupport::Initialize(rtInterf)) {
        RTSupport::Log::rt.Log(Logging::Level::ERROR, [](Stream::Output& out) {
            out << "failed to initialize runtime symbols" << Stream::endl;
        });
        return 1;
    }
    g_CJNativeInterfaceInstance = *rtInterf;

    interpInterf->version                  = INT_INTERPRETER_INTERFACE_VERSION;
    interpInterf->cjThreadSpecificDataSize = sizeof(Interpretation::Ectype);
    interpInterf->c2iStubStartAddr         = reinterpret_cast<uintptr_t>(&Asm::engine_c2i_call_pc_start);
    interpInterf->c2iStubEndAddr           = reinterpret_cast<uintptr_t>(&Asm::engine_c2i_call_pc_end);
    interpInterf->cjThreadOnStart          = &FiberStart;
    interpInterf->cjThreadOnDestroy        = &FiberDestroy;

    interpInterf->iterateFramesWithState   = &IterateFramesWithState;
    interpInterf->visitFrameRootsExpansion = &VisitFrameRootsExpansion;
    interpInterf->visitFrameRootsMarking   = &VisitFrameRootsMarking;
    interpInterf->visitFrameRootsAdjusting = &VisitFrameRootsAdjusting;
    interpInterf->visitGlobalRoots         = &VisitGlobalRoots;

    interpInterf->getFrameSize      = &GetFrameSize;
    interpInterf->frameInfoProvider = &FrameInfoProvider;
    interpInterf->frameDescProvider = &FrameDescProvider;

    interpInterf->landingPad = Asm::common_landing_pad;

    Asm::engine_carrier_specific_offset  = g_CJNativeInterfaceInstance.carrierSpecificOffset;
    Asm::engine_cjthread_specific_offset = g_CJNativeInterfaceInstance.cjThreadSpecificOffset;

    Asm::engine_tls_function              = g_CJNativeInterfaceInstance.getThreadLocalData;
    Asm::engine_throw_out_of_interpreter  = g_CJNativeInterfaceInstance.throwException;
    Asm::engine_newobject_function        = g_CJNativeInterfaceInstance.objectAlloc;
    Asm::engine_newobject_pinned_function = g_CJNativeInterfaceInstance.newPinnedObject;
    Asm::engine_newarray_function         = g_CJNativeInterfaceInstance.arrayAlloc;
    Asm::engine_stack_grow_stub           = g_CJNativeInterfaceInstance.stackGrowStub;
    Asm::engine_i2n_stub                  = g_CJNativeInterfaceInstance.i2nStub;

    RTSupport::InitBuiltinTypeInfos();

    g_BridgeInitialized = true;
    return 0;
}

} // extern "C"
