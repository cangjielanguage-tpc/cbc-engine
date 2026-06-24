#include "runtimesupport/impl/entrypoint.h"

#include <dlfcn.h>
#include <filesystem>
#include <mutex>
#include <system_error>

#include "RTInterface.h"
#include "asm_export.h"
#include "asm_trampolines.h"
#include "cbc/isa.h"
#include "cbc/isa_disasm.h"
#include "cbc_engine.h"
#include "cjnative.h"
#include "engine/engine.h"
#include "engine/options.h"
#include "engine/symlevel/io/filesystem.h"
#include "engine/symlevel/member_index.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/dependencies.h"
#include "engine/symlevel/reader.h"
#include "gc_support.h"
#include "interpreter/ectype.h"
#include "interpreter/function_handle.h"
#include "interpreter/loggers.h"
#include "runtimesupport/impl/rt_syms.h"
#include "utils/logger.h"
#include "utils/ostream.h"
#include "utils/rt_logger.h"

DYN_CJNativeInterface g_CJNativeInterfaceInstance;

static std::mutex g_InitializationGuard;
static bool g_Initialized;
static bool g_OptionsInitialized;
static bool g_Patched;

static void NativeLog(std::string message)
{
    auto logger = g_CJNativeInterfaceInstance.nativeLogger;
    if (logger == nullptr) {
        return;
    }

    static char tag[] = "Interpreter";
    logger(21, tag, message.data());
}

static const char* TypeInfoName(DYN_TypeInfo* ti)
{
    if (ti == nullptr) {
        return "<null>";
    }
    if (ti->typeInfoName == nullptr) {
        return "<unnamed>";
    }
    return ti->typeInfoName;
}

static void LogFunctionPointer(const char* prefix, size_t extDefIndex, size_t slotIndex, DYN_FuncPtr ptr)
{
    RTSupport::Log::rt.Log(Logging::Level::INFO, [prefix, extDefIndex, slotIndex, ptr](Stream::Output& out) {
        Dl_info info {};
        if (ptr != nullptr && dladdr(ptr, &info) != 0) {
            out.PrintFmtLn(
                "%s extDef[%zu].funcTable[%zu] = %p -> %s (%p) in %s",
                prefix,
                extDefIndex,
                slotIndex,
                ptr,
                info.dli_sname != nullptr ? info.dli_sname : "<unnamed>",
                info.dli_saddr,
                info.dli_fname != nullptr ? info.dli_fname : "<unknown>"
            );
            return;
        }

        out.PrintFmtLn("%s extDef[%zu].funcTable[%zu] = %p -> <unresolved>", prefix, extDefIndex, slotIndex, ptr);
    });
}

static void LogPatchExtensionDataTable(const char* phase, DYN_TypeInfo* ti)
{
    static constexpr size_t MAX_EXT_DEFS_TO_LOG     = 16;
    static constexpr uint16_t MAX_FUNC_SLOTS_TO_LOG = 4;

    RTSupport::Log::rt.Log(Logging::Level::INFO, [phase, ti](Stream::Output& out) {
        if (ti == nullptr) {
            out.PrintFmtLn("%s patch type info is null", phase);
            return;
        }

        out.PrintFmtLn(
            "%s patch type info %p name=%s validInheritNum=%u vExtensionDataStart=%p",
            phase,
            ti,
            TypeInfoName(ti),
            ti->validInheritNum,
            ti->vExtensionDataStart
        );
    });

    if (ti == nullptr || ti->vExtensionDataStart == nullptr) {
        return;
    }

    for (size_t extDefIndex = 0; extDefIndex < MAX_EXT_DEFS_TO_LOG; ++extDefIndex) {
        auto edef = ti->vExtensionDataStart[extDefIndex];
        if (edef == nullptr) {
            RTSupport::Log::rt.Log(Logging::Level::INFO, [phase, extDefIndex](Stream::Output& out) {
                out.PrintFmtLn("%s extDef[%zu] = <null>", phase, extDefIndex);
            });
            return;
        }

        RTSupport::Log::rt.Log(Logging::Level::INFO, [phase, extDefIndex, edef](Stream::Output& out) {
            if (edef->isInterfaceTypeInfo) {
                out.PrintFmtLn(
                    "%s extDef[%zu]=%p argNum=%u isInterfaceTypeInfo=%u flag=0x%02x funcTableSize=%u "
                    "target=%s interface=%s funcTable=%p",
                    phase,
                    extDefIndex,
                    edef,
                    edef->argNum,
                    edef->isInterfaceTypeInfo,
                    edef->flag,
                    edef->funcTableSize,
                    TypeInfoName(edef->ti),
                    TypeInfoName(edef->interfaceTypeInfo),
                    edef->funcTable
                );
                return;
            }

            out.PrintFmtLn(
                "%s extDef[%zu]=%p argNum=%u isInterfaceTypeInfo=%u flag=0x%02x funcTableSize=%u "
                "target=%s interfaceFn=%p funcTable=%p",
                phase,
                extDefIndex,
                edef,
                edef->argNum,
                edef->isInterfaceTypeInfo,
                edef->flag,
                edef->funcTableSize,
                TypeInfoName(edef->ti),
                edef->interfaceFn,
                edef->funcTable
            );
        });

        if (edef->funcTable == nullptr) {
            continue;
        }

        uint16_t slotLimit = edef->funcTableSize < MAX_FUNC_SLOTS_TO_LOG ? edef->funcTableSize : MAX_FUNC_SLOTS_TO_LOG;
        for (uint16_t slotIndex = 0; slotIndex < slotLimit; ++slotIndex) {
            LogFunctionPointer(phase, extDefIndex, slotIndex, edef->funcTable[slotIndex]);
        }
    }

    RTSupport::Log::rt.Log(Logging::Level::WARN, [](Stream::Output& out) {
        out << "extension-data logging stopped before a null terminator" << Stream::endl;
    });
}

static void InitEnvOpts()
{
    std::lock_guard guard(g_InitializationGuard);
    if (!g_OptionsInitialized) {
        Engine::InitEnvOptions();
        g_OptionsInitialized = true;
    }
}

static void DiscoverPatchCbcFromAppStorage()
{
    if (g_appStoragePath.empty() || !g_cbcPath.empty() || !g_patchCbc.empty()) {
        return;
    }

    auto cbcDir = std::filesystem::path(g_appStoragePath) / "cbc";

    std::error_code ec;
    if (!std::filesystem::is_directory(cbcDir, ec)) {
        if (ec) {
            NativeLog("failed to access app storage cbc directory: " + cbcDir.string());
        } else {
            NativeLog("app storage cbc directory does not exist: " + cbcDir.string());
        }
        return;
    }

    std::filesystem::directory_iterator it(cbcDir, std::filesystem::directory_options::skip_permission_denied, ec);
    if (ec) {
        NativeLog("failed to scan app storage cbc directory: " + cbcDir.string());
        return;
    }

    std::filesystem::path patchCbc;
    std::filesystem::directory_iterator end;
    while (it != end) {
        auto const& entry = *it;
        std::error_code fileEc;
        if (entry.is_regular_file(fileEc) && entry.path().extension() == ".cbc") {
            if (!patchCbc.empty()) {
                NativeLog("multiple .cbc files found in app storage cbc directory: " + cbcDir.string());
                return;
            }
            patchCbc = entry.path();
        }

        it.increment(ec);
        if (ec) {
            NativeLog("failed to scan app storage cbc directory: " + cbcDir.string());
            return;
        }
    }

    if (patchCbc.empty()) {
        NativeLog("no .cbc files found in app storage cbc directory: " + cbcDir.string());
        return;
    }

    g_patchCbc = patchCbc.string();
    NativeLog("using app storage patch cbc: " + g_patchCbc);
}

/// Initialize engine from launcher.
static void EnsureEngineInitialized(std::string cbcFile)
{
    InitEnvOpts();
    std::lock_guard guard(g_InitializationGuard);
    if (g_Initialized) {
        return;
    }

    RTSupport::Log::rt.Log(Logging::Level::INFO, [](Stream::Output& out) {
        out << "start engine init" << Stream::endl;
    });

    Engine::Loader loader;

    auto file = IO::TryOpenFile(cbcFile);
    if (file.has_value()) {
        loader.Load(std::move(file.value()), cbcFile);
    } else {
        RTSupport::Log::rt.Log(Logging::Level::WARN, [&cbcFile](Stream::Output& out) {
            out.PrintFmtLn("engine init: no such file or directory %s", cbcFile.c_str());
        });
    }
    loader.Build();

    RTSupport::Log::rt.Log(Logging::Level::INFO, [](Stream::Output& out) {
        out << "end engine init" << Stream::endl;
    });

    g_Initialized = true;
}

static void PerformPatching()
{
    EnsureEngineInitialized(g_patchCbc);

    std::lock_guard guard(g_InitializationGuard);
    if (g_Patched) {
        return;
    }

    RTSupport::Log::rt.Log(Logging::Level::INFO, [](Stream::Output& out) {
        out << "start patching" << Stream::endl;
    });

    auto& engine = Engine::GetEngineInstance();

    Engine::Session session(engine);

    auto& fuhManager = Interpretation::FunctionHandleManager::Of(engine);

    for (auto& file : engine.Files()) {
        // TODO: list patches in CBC file header
        auto ti = file.GetTypeIndex();
        ti.ForEach(session, [&](Symlevel::TypeDefinition& def) {
            if (!def.GetFlags().Is(Symlevel::TypeFlag::PATCH)) {
                return;
            }

            auto pkgName = Symlevel::Reader::Read(session, def.GetName());
            pkgName = pkgName.substr(3);

            RTSupport::Log::rt.Log(Logging::Level::INFO, [&pkgName](Stream::Output& out) {
                out.PrintFmtLn("patching package %s", pkgName);
            });

            auto patchPrefix = "$" + std::string(pkgName);

            auto patchFlagName = patchPrefix + "$packageInit$GVF";
            auto patchClassName = std::string(pkgName) + ":" + patchPrefix + "$PackageInitPatch$GC";

            // Get patched type info
            auto ti = g_CJNativeInterfaceInstance.typeInfo(patchClassName.c_str());
            if (ti == nullptr) {
                RTSupport::Log::rt.Log(Logging::Level::ERROR, [&patchClassName](Stream::Output& out) {
                    out.PrintFmtLn("patch type info not found: %s", patchClassName.c_str());
                });
                return;
            }

            RTSupport::Log::rt.Log(Logging::Level::INFO, [&patchClassName](Stream::Output& out) {
                out.PrintFmtLn("patch type info found: %s", patchClassName.c_str());
            });

            LogPatchExtensionDataTable("before patch", ti);

            // Corresponding extension def (TODO: check it)
            if (ti->vExtensionDataStart == nullptr || ti->vExtensionDataStart[1] == nullptr) {
                RTSupport::Log::rt.Log(Logging::Level::ERROR, [](Stream::Output& out) {
                    out << "patch extension data vExtensionDataStart[1] is not available" << Stream::endl;
                });
                return;
            }
            auto edef = ti->vExtensionDataStart[1];

            def.GetMethods().ForEach(session, [&](Symlevel::MethodDefinition& mdef) {

                auto idx = -1;
                if (mdef.GetFlags().Is(Symlevel::MethodFlag::PKG_INIT)) {
                    idx = 0;
                }
                if (mdef.GetFlags().Is(Symlevel::MethodFlag::LIT_INIT)) {
                    idx = 1;
                }

                if (idx != -1) {

                    RTSupport::Log::rt.Log(Logging::Level::INFO, [&idx, &session, &mdef](Stream::Output& out) {
                        auto funcName = Symlevel::Reader::Read(session, mdef.Name());
                        out.PrintFmtLn("patching funcTable[%d] with %s", idx, funcName);
                    });

                    auto fuh = fuhManager.AcquireTagged(session, mdef.GetIdentifier());
                    auto ptr = fuhManager.GetFunctionPtrForDirectCall(fuh);

                    if (edef->funcTable == nullptr || idx >= edef->funcTableSize) {
                        RTSupport::Log::rt.Log(Logging::Level::ERROR, [&idx, edef](Stream::Output& out) {
                            out.PrintFmtLn(
                                "cannot patch funcTable[%d]: funcTable=%p funcTableSize=%u",
                                idx,
                                edef->funcTable,
                                edef->funcTableSize
                            );
                        });
                        return;
                    }

                    LogFunctionPointer("patch old", 1, idx, edef->funcTable[idx]);
                    LogFunctionPointer("patch new", 1, idx, ptr);
                    edef->funcTable[idx] = ptr;
                    LogFunctionPointer("patch stored", 1, idx, edef->funcTable[idx]);
                }
            });

            LogPatchExtensionDataTable("after patch", ti);

            // Set patched flag
            auto& deps = file.GetDependencies();
            auto flag = deps.FindTarget(patchFlagName);
            if (flag == nullptr) {
                RTSupport::Log::rt.Log(Logging::Level::ERROR, [&patchFlagName](Stream::Output& out) {
                    out.PrintFmtLn("patch flag field not found: %s", patchFlagName.c_str());
                });
                return;
            }

            RTSupport::Log::rt.Log(Logging::Level::INFO, [&patchFlagName](Stream::Output& out) {
                out.PrintFmtLn("patch flag field found: %s", patchFlagName.c_str());
            });

            *(bool*) flag = true;
        });

    }

    RTSupport::Log::rt.Log(Logging::Level::INFO, [](Stream::Output& out) {
        out << "end patching" << Stream::endl;
    });

    g_Patched = true;
}

static void FiberStart(DYN_CJThreadSpecificData* data) { *data = nullptr; }

#if defined(__APPLE__)
    #define FIBER_INIT_ASM_LABEL "_engine_fiber_data_init"
#else
    #define FIBER_INIT_ASM_LABEL "engine_fiber_data_init"
#endif

extern "C" Interpretation::Ectype* FiberDataInit(DYN_CJThreadSpecificData* data) __asm__(FIBER_INIT_ASM_LABEL);

Interpretation::Ectype* FiberDataInit(DYN_CJThreadSpecificData* data)
{
    ASSERTION(*data == nullptr, "Incorrect data value: %p", *data);

    auto ectype = new Interpretation::Ectype();
    if (!ectype) {
        FATAL("Ectype allocation error");
    }

    *data = ectype;

    RTSupport::Log::rt.Log(Logging::Level::INFO, [&data](Stream::Output& out) {
        out.PrintFmtLn("fiber init, fsd addr: %p, ectype addr: %p", data, *data);
    });

    return ectype;
}

static void FiberDestroy(DYN_CJThreadSpecificData* data)
{
    if (*data == nullptr) {
        // ectype wasn't initialized for this fiber, nothing to do here
        return;
    }

    RTSupport::Log::rt.Log(Logging::Level::INFO, [&data](Stream::Output& out) {
        out.PrintFmtLn("fiber destroy, fsd addr: %p, ectype addr: %p", data, *data);
    });
    delete static_cast<Interpretation::Ectype*>(*data)->Checked();
}

static void IterateFramesWithState(
    DYN_CJThreadSpecificData threadSpecificData, void (*callback)(DYN_VisitingState, void*), void* ctx
)
{
    if (g_Initialized) {
        GCSupport::IterateFramesWithState(threadSpecificData, callback, ctx);
    }
}

static void VisitFrameRootsMarking(DYN_VisitingState state, INT_FrameDesc frame_desc, DYN_RootVisitor root_visitor)
{
    if (g_Initialized) {
        GCSupport::VisitGCFrameRoots(state, frame_desc, root_visitor);
    }
}

static void VisitFrameRootsAdjusting(
    DYN_VisitingState state,
    INT_FrameDesc frame_desc,
    DYN_RootVisitor root_visitor,
    DYN_DerivedPtrVisitor derived_ptr_visitor
)
{
    if (g_Initialized) {
        GCSupport::VisitGCFrameRoots(state, frame_desc, root_visitor);
    }
}

static void VisitFrameRootsExpansion(
    DYN_VisitingState state,
    INT_FrameDesc frameDesc,
    DYN_RootVisitor stackPtrVisitor,
    DYN_DerivedPtrVisitor derivedPtrVisitor
)
{
    /* no-op */
}

static void VisitGlobalRoots(DYN_RootVisitor visitor)
{
    if (g_Initialized) {
        GCSupport::VisitGlobalRoots(visitor);
    }
}

extern "C" {
/// This symbol is exported to the runtime, which would initialize engine.
CBC_EXPORT int interpreter_bridge_init(
    struct INT_InterpreterInterface* interpInterf,
    struct DYN_CJNativeInterface* rtInterf,
    int size,
    INT_InterpreterArgs options
);

CBC_EXPORT void engine_set_cbcpath(char const* cbcPath) { g_cbcPath = cbcPath; }

CBC_EXPORT void engine_set_main_cbc(char const* mainCbc) { g_mainCbc = mainCbc; }

CBC_EXPORT void engine_initialize() { EnsureEngineInitialized(g_mainCbc); }

CBC_EXPORT void engine_enable_dasm() { Interpretation::Log::preparation.SetLogLevel(Logging::Level::TRACE); }

CBC_EXPORT void engine_enable_raw_dasm()
{
    engine_enable_dasm();
    Cbc::EnableRawDisasm();
}

CBC_EXPORT void* engine_get_entrypoint_trampoline(void)
{
    ASSERTION(g_Initialized, "Engine is not initialized");

    auto& engine = Engine::GetEngineInstance();

    Engine::Session session(engine);

    auto main = engine.FindMain(session, g_mainCbc);
    if (!main.has_value()) {
        return nullptr;
    }
    auto& fuhManager = Interpretation::FunctionHandleManager::Of(engine);

    auto fuh = fuhManager.AcquireTagged(session, main.value());
    return fuhManager.GetFunctionPtrForDirectCall(fuh);
}

CBC_EXPORT int interpreter_bridge_init(
    struct INT_InterpreterInterface* interpInterf,
    struct DYN_CJNativeInterface* rtInterf,
    int size,
    INT_InterpreterArgs options
)
{
    static_assert(std::is_same_v<decltype(&interpreter_bridge_init), INT_InitInterpreter>);
    if (rtInterf == nullptr || rtInterf->version != DYN_CJNATIVE_INTERFACE_VERSION) {
        RTSupport::Log::rt.Log(Logging::Level::ERROR, [rtInterf](Stream::Output& out) {
            out.PrintFmtLn(
                "Unexpected DYN_CJNativeInterface version: expected %d, actual %lld",
                DYN_CJNATIVE_INTERFACE_VERSION,
                static_cast<long long>(rtInterf != nullptr ? rtInterf->version : -1)
            );
        });
        return 1;
    }

    g_CJNativeInterfaceInstance = *rtInterf;

    NativeLog("Interpreter bridge init started");

    // Order matters
    InitEnvOpts();
    Engine::g_table.ParseAndSet(size, options);

    DiscoverPatchCbcFromAppStorage();

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

    Asm::engine_carrier_specific_offset  = g_CJNativeInterfaceInstance.carrierSpecificOffset;
    Asm::engine_cjthread_specific_offset = g_CJNativeInterfaceInstance.cjThreadSpecificOffset;

    Asm::engine_newobject_function = g_CJNativeInterfaceInstance.objectAlloc;
    Asm::engine_newarray_function = g_CJNativeInterfaceInstance.arrayAlloc;
    RTSupport::Initialize(&g_CJNativeInterfaceInstance);

    if (!g_patchCbc.empty()) {
        PerformPatching();
    }

    NativeLog("Interpreter bridge init finished");

    return 0;
}

} // extern "C"
