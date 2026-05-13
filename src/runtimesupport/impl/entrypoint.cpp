#include "runtimesupport/impl/entrypoint.h"

#include <filesystem>
#include <mutex>

#include "RTInterface.h"
#include "asm_export.h"
#include "asm_trampolines.h"
#include "cbc/isa_disasm.h"
#include "cbc_engine.h"
#include "cjnative.h"
#include "engine/engine.h"
#include "engine/options.h"
#include "engine/statics_manager.h"
#include "engine/symlevel/io/filesystem.h"
#include "interpreter/ectype.h"
#include "interpreter/function_handle.h"
#include "interpreter/loggers.h"
#include "runtimesupport/impl/rt_syms.h"
#include "utils/logger.h"
#include "utils/rt_logger.h"

DYN_CJNativeInterfaceT g_CJNativeInterfaceInstance;

static std::mutex g_InitializationGuard;
static bool g_Initialized;
static bool g_OptionsInitialized;

static void InitEnvOpts()
{
    std::lock_guard guard(g_InitializationGuard);
    if (!g_OptionsInitialized) {
        Engine::InitEnvOptions();
        g_OptionsInitialized = true;
    }
}

/// Initialize engine from launcher.
static void EnsureEngineInitialized()
{
    InitEnvOpts();
    std::lock_guard guard(g_InitializationGuard);
    if (g_Initialized) {
        return;
    }

    Engine::InitEnvOptions();
    Engine::Loader loader;
    loader.Load(IO::OpenFile(std::filesystem::path(g_mainCbc)), g_mainCbc);
    loader.Build();
    g_Initialized = true;
}

static void FiberStart(DYN_CJThreadSpecificDataT* data) { /* no-op */ }

static void FiberDestroy(DYN_CJThreadSpecificDataT* data) { /* TODO: ectype cleanup */ }

static void IterateFramesWithState(
    DYN_CJThreadSpecificDataT threadSpecificData, void (*callback)(DYN_VisitingStateT, void*), void* ctx
)
{
    RTSupport::Log::gc.Stream(Logging::Level::INFO)
        .PrintFmt("start scanning frames, thread spec data = %p\n", threadSpecificData);

    DYN_VisitingStateT state = nullptr; // TODO implement
    callback(state, ctx);

    RTSupport::Log::gc.Stream(Logging::Level::INFO)
        .PrintFmt("end scanning frames, thread spec data = %p\n", threadSpecificData);
}

static void VisitGCFrameRoots(DYN_FrameDescT frame_desc, DYN_RootVisitorT root_visitor)
{
    using namespace Interpretation;
    const auto readerOffset = LOCAL_SLOTS_OFFSET + READER_SLOTS_SIZE;

    auto fuh    = *reinterpret_cast<DynamicFunctionHandle**>((uint8_t*)frame_desc.fp - FUH_SLOT_OFFSET);
    auto reader = reinterpret_cast<Decoder::ByteReader*>((uint8_t*)frame_desc.fp - readerOffset);
    auto bc     = NOTNULL(fuh->bytecode.load());

    uint32_t curPos = reinterpret_cast<uintptr_t>(reader->Cursor()) - reinterpret_cast<uintptr_t>(bc->code.bytecode);

    const ReferenceInfo* refInfo = nullptr;
    for (auto& info : bc->referenceInfos) {
        if (info.rewrittenPos == curPos) {
            refInfo = &info;
            break;
        }
    }

    auto slotsStartAddr = ((uint8_t*)frame_desc.fp) - (readerOffset + bc->frameSize);

    Log::interpretation.Stream(Logging::Level::INFO)
        .PrintFmt(
            "start visiting frame (fuh=%p, fp=%p, pos=%p, slots_addr=%p)\n", fuh, frame_desc, curPos, slotsStartAddr
        );

    for (auto& refSlotOffset : NOTNULL(refInfo)->refSlotOffsets) {
        uintptr_t* refLocation = reinterpret_cast<uintptr_t*>(slotsStartAddr + refSlotOffset);
        RTSupport::Log::gc.Stream(Logging::Level::TRACE).PrintFmt("visiting %p, value=%p\n", refLocation, *refLocation);
        g_CJNativeInterfaceInstance.visitRootFromInterpreter(root_visitor, refLocation);
    }

    Log::interpretation.Stream(Logging::Level::INFO).PrintFmt("end visiting frame (fuh=%p)\n", fuh);
}

static void VisitFrameRootsMarking(DYN_VisitingStateT state, DYN_FrameDescT frame_desc, DYN_RootVisitorT root_visitor)
{
    // TODO scan saved regs
    VisitGCFrameRoots(frame_desc, root_visitor);
}

static void VisitFrameRootsAdjusting(
    DYN_VisitingStateT state,
    DYN_FrameDescT frame_desc,
    DYN_RootVisitorT root_visitor,
    DYN_DerivedPtrVisitorT derived_ptr_visitor
)
{
    // scan saved regs
    VisitGCFrameRoots(frame_desc, root_visitor);
}

static void VisitFrameRootsExpansion(
    DYN_VisitingStateT state,
    DYN_FrameDescT frameDesc,
    DYN_RootVisitorT stackPtrVisitor,
    DYN_DerivedPtrVisitorT derivedPtrVisitor
)
{
    /* no-op */
}

static void VisitGlobalRoots(DYN_RootVisitorT visitor)
{
    RTSupport::Log::gc.Stream(Logging::Level::INFO).PrintFmt("start visiting global roots\n");

    auto& engine = Engine::GetEngineInstance();
    Engine::StaticsManager::Of(engine).VisitRefLocations([visitor](Engine::RefLocation* refLocation) {
        RTSupport::Log::gc.Stream(Logging::Level::TRACE).PrintFmt("visiting %p, value=%p\n", refLocation, *refLocation);
        g_CJNativeInterfaceInstance.visitRootFromInterpreter(visitor, refLocation);
    });

    RTSupport::Log::gc.Stream(Logging::Level::INFO).PrintFmt("end visiting global roots\n");
}

extern "C" {
/// This symbol is exported to the runtime, which would initialize engine.
CBC_EXPORT int interpreter_bridge_init(
    struct DYN_InterpreterInterfaceT* interpInterf,
    struct DYN_CJNativeInterfaceT* rtInterf,
    int size,
    const char* const* options
);

CBC_EXPORT void engine_set_cbcpath(char const* cbcPath) { g_cbcPath = cbcPath; }

CBC_EXPORT void engine_set_main_cbc(char const* mainCbc) { g_mainCbc = mainCbc; }

CBC_EXPORT void engine_initialize() { EnsureEngineInitialized(); }

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
    struct DYN_InterpreterInterfaceT* interpInterf,
    struct DYN_CJNativeInterfaceT* rtInterf,
    int size,
    const char* const* options
)
{
    static_assert(std::is_same_v<decltype(&interpreter_bridge_init), DYN_InitRt>);

    // Order matters
    InitEnvOpts();
    Engine::g_table.ParseAndSet(size, options);

    g_CJNativeInterfaceInstance            = *rtInterf;
    interpInterf->version                  = 1;
    interpInterf->cjThreadSpecificDataSize = sizeof(Interpretation::Ectype);
    interpInterf->iteratorSize             = 0; // FIXME: remove
    interpInterf->c2iStubStartAddr         = reinterpret_cast<uintptr_t>(&Asm::engine_c2i_call_pc_start);
    interpInterf->c2iStubEndAddr           = reinterpret_cast<uintptr_t>(&Asm::engine_c2i_call_pc_end);
    interpInterf->cjThreadStart            = &FiberStart;
    interpInterf->cjThreadDestroy          = &FiberDestroy;

    interpInterf->iterateFramesWithState   = &IterateFramesWithState;
    interpInterf->visitFrameRootsExpansion = &VisitFrameRootsExpansion;
    interpInterf->visitFrameRootsMarking   = &VisitFrameRootsMarking;
    interpInterf->visitFrameRootsAdjusting = &VisitFrameRootsAdjusting;
    interpInterf->visitGlobalRoots         = &VisitGlobalRoots;

    Asm::engine_newobject_function = g_CJNativeInterfaceInstance.objectAlloc;
    RTSupport::Initialize(&g_CJNativeInterfaceInstance);

    return 0;
}

} // extern "C"
