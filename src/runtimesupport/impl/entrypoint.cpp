#include "runtimesupport/impl/entrypoint.h"

#include <algorithm>
#include <mutex>

#include "RTInterface.h"
#include "asm_export.h"
#include "asm_trampolines.h"
#include "cbc/isa_disasm.h"
#include "cbc_engine.h"
#include "cjnative.h"
#include "engine/engine.h"
#include "engine/options.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/dependencies.h"
#include "engine/symlevel/io/filesystem.h"
#include "engine/symlevel/member_index.h"
#include "engine/symlevel/reader.h"
#include "gc_support.h"
#include "interpreter/ectype.h"
#include "interpreter/function_handle.h"
#include "interpreter/interpretation_loop.h"
#include "interpreter/implicit_exceptions.h"
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

static void InitEnvOpts()
{
    std::lock_guard guard(g_InitializationGuard);
    if (!g_OptionsInitialized) {
        Engine::InitEnvOptions();
        g_OptionsInitialized = true;
    }
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
                out << "patching package " << pkgName << Stream::endl;
            });

            auto patchPrefix = "$" + std::string(pkgName);

            auto patchFlagName = patchPrefix + "$packageInit$GVF";
            auto patchClassName = std::string(pkgName) + ":" + patchPrefix + "$PackageInitPatch$GC";

            // Get patched type info
            auto ti = g_CJNativeInterfaceInstance.typeInfo(patchClassName.c_str());
            if (ti == nullptr) {
                RTSupport::Log::rt.Log(Logging::Level::ERROR, [&patchClassName](Stream::Output& out) {
                    out << "patch type info not found: " << patchClassName << Stream::endl;
                });
                return;
            }

            RTSupport::Log::rt.Log(Logging::Level::INFO, [&patchClassName](Stream::Output& out) {
                out << "patch type info found: " << patchClassName << Stream::endl;
            });

            // Corresponding extension def (TODO: check it)
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
                        out << "patching funcTable[" << idx << "] with " << funcName << Stream::endl;
                    });

                    auto fuh = fuhManager.AcquireTagged(session, mdef.GetIdentifier());
                    auto ptr = fuhManager.GetFunctionPtrForDirectCall(fuh);

                    edef->funcTable[idx] = ptr;
                }
            });

            // Set patched flag
            auto& deps = file.GetDependencies();
            auto flag = deps.FindTarget(patchFlagName);
            if (flag == nullptr) {
                RTSupport::Log::rt.Log(Logging::Level::ERROR, [&patchFlagName](Stream::Output& out) {
                    out << "patch flag field not found: " << patchFlagName << Stream::endl;
                });
                return;
            }

            RTSupport::Log::rt.Log(Logging::Level::INFO, [&patchFlagName](Stream::Output& out) {
                out << "patch flag field found: " << patchFlagName << Stream::endl;
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

// TODO implement
static void FrameInfoProvider(DYN_FramePointer fp, DYN_InstructionPointer ip, INT_InterpretedFrameInfo* info)
{
    info->bcPos = 0;
    info->fuh   = nullptr;
    return;
}

extern "C" {
/// This symbol is exported to the runtime, which would initialize engine.
CBC_EXPORT int interpreter_bridge_init(
    struct INT_InterpreterInterface* interpInterf,
    struct DYN_CJNativeInterface* rtInterf,
    int size,
    const char** options
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
    const char** options
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

    // Order matters
    InitEnvOpts();
    Engine::g_table.ParseAndSet(size, options);

    g_CJNativeInterfaceInstance            = *rtInterf;
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

    interpInterf->frameInfoProvider = &FrameInfoProvider;

    interpInterf->landingPad = Asm::common_landing_pad;

    Asm::engine_carrier_specific_offset  = g_CJNativeInterfaceInstance.carrierSpecificOffset;
    Asm::engine_cjthread_specific_offset = g_CJNativeInterfaceInstance.cjThreadSpecificOffset;

    Asm::engine_tls_function = g_CJNativeInterfaceInstance.getThreadLocalData;
    Asm::engine_throw_out_of_interpreter = g_CJNativeInterfaceInstance.throwException;
    Asm::engine_newobject_function = g_CJNativeInterfaceInstance.objectAlloc;
    Asm::engine_newarray_function = g_CJNativeInterfaceInstance.arrayAlloc;
    RTSupport::Initialize(&g_CJNativeInterfaceInstance);

    if (!g_patchCbc.empty()) {
        PerformPatching();
    }
    // If CBC patch is not found, engine will be left uninitialized.
    // But Cangjie runtime will still be calling provided interpreter callbacks.
    // So behavior of INT_InterpreterInterface callbacks with uninitialized engine should be changed:
    // 1. GC roots visitors should be no-op (since no roots can be created without engine initialization).

    {
        using namespace Interpretation;
        auto getTypeInfo                  = g_CJNativeInterfaceInstance.typeInfo;
        builtinTypeInfos[BUILTIN_BOOLEAN] = RTSupport::TypeInfo(getTypeInfo("Bool"));
        builtinTypeInfos[BUILTIN_U8]      = RTSupport::TypeInfo(getTypeInfo("UInt8"));
        builtinTypeInfos[BUILTIN_U16]     = RTSupport::TypeInfo(getTypeInfo("UInt16"));
        builtinTypeInfos[BUILTIN_U32]     = RTSupport::TypeInfo(getTypeInfo("UInt32"));
        builtinTypeInfos[BUILTIN_U64]     = RTSupport::TypeInfo(getTypeInfo("UInt64"));
        builtinTypeInfos[BUILTIN_I8]      = RTSupport::TypeInfo(getTypeInfo("Int8"));
        builtinTypeInfos[BUILTIN_I16]     = RTSupport::TypeInfo(getTypeInfo("Int16"));
        builtinTypeInfos[BUILTIN_I32]     = RTSupport::TypeInfo(getTypeInfo("Int32"));
        builtinTypeInfos[BUILTIN_I64]     = RTSupport::TypeInfo(getTypeInfo("Int64"));
        builtinTypeInfos[BUILTIN_F16]     = RTSupport::TypeInfo(getTypeInfo("Float16"));
        builtinTypeInfos[BUILTIN_F32]     = RTSupport::TypeInfo(getTypeInfo("Float32"));
        builtinTypeInfos[BUILTIN_F64]     = RTSupport::TypeInfo(getTypeInfo("Float64"));
    }

    return 0;
}

} // extern "C"
