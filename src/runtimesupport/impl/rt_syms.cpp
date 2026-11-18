#include "rt_syms.h"
#include "runtimesupport/impl/asm_trampolines.h"
#include "utils/logger.h"
#include "utils/ostream.h"
#include "utils/rt_logger.h"
#include "utils/sharedobj.h"
#include <dlfcn.h>
#include <string_view>

#include "engine/engine.h"
#include "interpreter/adapters.h"
#include "interpreter/function_handle.h"
#include "utils/assertion.h"

namespace RTSupport {

DYN_WriteStructFieldFn WriteStructField;
DYN_ReadStructFieldFn ReadStructField;
DYN_WriteGenericFieldFn WriteGeneric;
DYN_ReadWeakRefFn ReadWeakRef;

namespace {

// bootstrap cycle of implicit exceptions:
//  (a) to throw implicit exception we need to create exception object (execute Cangjie code)
//  (b) some Exceptions (OutOfMemory, OutOfBounds) defined in std.core which also may be compiled to CBC bytecode
//
// solution:
//  1. lazily resolve callbacks as FuH -- direct call trampolines or global symbols
//  2. implicit exception thrower compiled to *.chir during toolchain packing and always prepended to compilation scope (see cbc-compiler)
//
// caveats:
//  - lazy initialization in mutlithreaded context
//  - abi-specific support in trampolines.S
//

// External fallback source for `fallbackToStdCore` specs. Under
// STATIC_HELPER std.core is statically linked into the application itself,
// so the executable's symbol table is the fallback source.
#if defined(STATIC_HELPER)
constexpr std::string_view STD_CORE_LIB_NAME = "the application";

Utils::SharedObject* StdCoreLibrary()
{
    static Utils::SharedObject stdCore = Utils::SharedObject::OpenCurrentExecutable();
    return &stdCore;
}
#else
constexpr std::string_view STD_CORE_LIB_NAME = "libcangjie-std-core.so";

Utils::SharedObject* StdCoreLibrary()
{
    static Utils::SharedObject stdCore = Utils::SharedObject::Open(std::string_view(STD_CORE_LIB_NAME));
    return &stdCore;
}
#endif

// Optional side effect of the resolution (e.g. installing an asm trampoline
// callback), applied before the resolved address is published to other threads.
using ResolvedCallback = void (*)(void* symbol);

// Runtime callback to resolve, may be
//   - a direct call trampoline of the cbc method found in the image, or
//   - a raw symbol from AOT compiled libcangjie-std-core.so (when `fallbackToStdCore`)
struct MethodSpec {
    std::string_view typeName;
    const char* linkageName;
    const char* errorMessage;
    bool fallbackToStdCore;
};

// Fatal exit for a `fallbackToStdCore` spec: the source is std-core, either
// the shared library or the application itself (STATIC_HELPER).
[[noreturn]] void FatalStdCoreNotFound(const MethodSpec& spec)
{
    FATAL("%s was not found in the image (type %.*s) and %s could not be opened", spec.errorMessage,
        static_cast<int>(spec.typeName.size()), spec.typeName.data(), STD_CORE_LIB_NAME.data());
}

[[noreturn]] void FatalStdCoreSymNotFound(const MethodSpec& spec, const std::string& error)
{
    FATAL("%s was not found in the image (type %.*s) nor in %s: %s", spec.errorMessage,
        static_cast<int>(spec.typeName.size()), spec.typeName.data(), STD_CORE_LIB_NAME.data(),
        error.c_str());
}

// Stateless resolution: callers keep the result in a function-local static,
// which provides thread-safe lazy initialization.
void* ResolveMethod(const MethodSpec& spec, ResolvedCallback callback = nullptr)
{
    void* resolved = nullptr;
    {
        auto& engine     = Engine::GetEngineInstance();
        Engine::Session session(engine);
        auto& fuhManager = Interpretation::FunctionHandleManager::Of(engine);

        auto type = engine.FindType(session, spec.typeName);
        if (type.has_value()) {
            auto method = engine.FindMethodInType(session, *type, spec.linkageName);
            if (method.has_value()) {
                auto taggedHandle = fuhManager.AcquireTagged(session, *method);
                auto* dynFuh      = std::get_if<Interpretation::DynamicFunctionHandle*>(&taggedHandle);
                if (dynFuh != nullptr) {
                    resolved = Interpretation::GetDirectCallTrampoline(*dynFuh);
                }
            }
        }
    }

    if (resolved == nullptr && spec.fallbackToStdCore) {
        auto* stdCore = StdCoreLibrary();
        if (!stdCore->IsOpened()) {
            FatalStdCoreNotFound(spec);
        }
        auto search = stdCore->SearchSym(spec.linkageName);
        if (search.symbol == nullptr) {
            FatalStdCoreSymNotFound(spec, search.error);
        }
        resolved = search.symbol;
    }

    if (resolved == nullptr) {
        FATAL(
            "%s was not found in the image as a cbc method (type %.*s, method %s)",
            spec.errorMessage,
            static_cast<int>(spec.typeName.size()),
            spec.typeName.data(),
            spec.linkageName
        );
    }

    if (callback != nullptr) {
        callback(resolved);
    }
    return resolved;
}

void InstallThreadHandleSetter(void* setter)
{
    Asm::engine_set_runtime_cjthread_handle_function = setter;
}

// Temporary: lazy resolution here is a stopgap to let std.core compiled to CBC
// move forward.
// FIXME(#270): rework runtimesupport / rt_syms.
constexpr MethodSpec THROWER_SPEC{
    "$P$cangjie.runtime.cbcengine.helper",
    "_CN32cangjie.runtime.cbcengine.helper22throwImplicitExceptionHl",
    "ImplicitExceptionThrower",
    false,
};

constexpr MethodSpec FUTURE_EXECUTE_SPEC{
    "std.core:Future",
    "_CNat6FutureIG_E7executeHv",
    "Future.execute",
    true,
};

constexpr MethodSpec THREAD_HANDLE_SETTER_SPEC{
    "std.core:Thread",
    "_CNat6Thread24setRuntimeCJThreadHandleHPu",
    "Thread.setRuntimeCJThreadHandle",
    true,
};

} // namespace

void* ResolveFutureExecute()
{
    static void* func = ResolveMethod(FUTURE_EXECUTE_SPEC);
    return func;
}

void* ResolveThreadHandleSetter()
{
    static void* func = ResolveMethod(THREAD_HANDLE_SETTER_SPEC, &InstallThreadHandleSetter);
    return func;
}

void* ResolveImplicitExceptionThrower()
{
    static void* func = ResolveMethod(THROWER_SPEC);
    return func;
}

} // namespace RTSupport

// Installed as `Asm::engine_implicit_exception_thrower` and called from the
// interpreter thunk (see `.Lthunk_op_ThrowImplicit`). Resolves
// `throwImplicitException` and returns its direct call trampoline; the thunk
// tail-calls it with the exception kind stashed in a callee-saved register
// across this call, so no C frame is left on the stack. The interpreted
// thrower throws the Cangjie exception and never returns.
extern "C" void* engine_throw_implicit_exception()
{
    return RTSupport::ResolveImplicitExceptionThrower();
}

namespace RTSupport {

bool Initialize(DYN_CJNativeInterface* interf)
{
    auto anySym = reinterpret_cast<void*>(interf->stackGrowStub);

    Dl_info info;
    int code = dladdr(anySym, &info);
    if (code == 0) {
        Log::init.Stream(Logging::Level::ERROR) << "dladdr failed to find rt info" << Stream::endl;
        return false;
    }
    auto handle = Utils::SharedObject::Open(std::string_view(info.dli_fname));
    if (!handle.IsOpened()) {
        Log::init.Stream(Logging::Level::ERROR) << "failed to open lib " << info.dli_fname << Stream::endl;
        return false;
    }

    // verify that we didn't opened new library (any other exported symbol can be used).
    auto stackGrowStub = handle.SearchSym("CJ_MCC_StackGrowStub").symbol;

    if (stackGrowStub != interf->stackGrowStub) {
        auto& stream = Log::init.Stream(Logging::Level::ERROR);
        stream << "incorrect stack grow stub address ";
        stream << interf->stackGrowStub << " " << stackGrowStub << Stream::endl;
        return false;
    }

    // TODO: maybe we should add readweakref to runtime interface
    auto readWeakRef = handle.SearchSym("CJ_MCC_ReadWeakRef").symbol;
    if (readWeakRef == nullptr) {
        Log::init.Stream(Logging::Level::ERROR) << "failed to find symbol CJ_MCC_ReadWeakRef" << Stream::endl;
        return false;
    }

    Asm::engine_implicit_exception_thrower = &engine_throw_implicit_exception;

    Asm::engine_newthread_nret_function = interf->newCJThreadNoReturn;
    Asm::engine_read_generic            = interf->readGenericField;

    WriteGeneric     = interf->writeGenericField;
    WriteStructField = interf->writeStructField;
    ReadStructField  = interf->readStructField;
    ReadWeakRef      = reinterpret_cast<DYN_ReadWeakRefFn>(readWeakRef);
    return true;
}

} // namespace RTSupport
