#include "rt_syms.h"
#include "runtimesupport/impl/asm_trampolines.h"
#include "utils/assertion.h"
#include "utils/logger.h"
#include "utils/ostream.h"
#include "utils/rt_logger.h"
#include <cstdint>
#include <dlfcn.h>
#include <optional>

namespace RTSupport {

DYN_WriteStructFieldFn WriteStructField;
DYN_ReadStructFieldFn ReadStructField;

DYN_WriteGenericFieldFn WriteGeneric;

// merge with LibHandle
struct Handle {
    void* handle;
    std::string name;

    Handle() : handle(nullptr) {}

    Handle(void* handle, std::string&& name) : handle(handle), name(std::move(name)) {}

    Handle(Handle const& handle) = delete;

    Handle(Handle&& handle) : handle(handle.handle) { handle.handle = nullptr; }

    Handle& operator=(Handle&& other)
    {
        if (handle != nullptr) {
            FATAL("Trying to rewrite existing handle");
        }
        name         = std::move(other.name);
        handle       = other.handle;
        other.handle = nullptr;
        return *this;
    }

    static std::optional<Handle> Open(std::string&& str)
    {
        void* handle = dlopen(str.c_str(), RTLD_LAZY);
        if (handle) {
            return Handle(handle, std::move(str));
        } else {
            return std::nullopt;
        }
    }

    void* Sym(char const* str)
    {
        auto res = dlsym(handle, str);
        if (!res) {
            Log::init.Stream(Logging::Level::ERROR) << '{' << name << "} failed to find " << str << Stream::endl;
        }
        return res;
    }

    template <typename T> T Func(char const* str) { return reinterpret_cast<T>(Sym(str)); }

    ~Handle()
    {
        if (handle) {
            dlclose(handle);
        }
    }
};

// Prolongs lifetime of helper lib handle after finish of Initialize.
Handle g_helperLibHandle;

static void* LookupSymbol(void* handle, char const* handleName, char const* symbolName)
{
    dlerror();
    auto* symbol = dlsym(handle, symbolName);
    if (symbol == nullptr) {
        auto* error  = dlerror();
        auto& stream = Log::init.Stream(Logging::Level::ERROR);
        stream << '{' << handleName << "} failed to find " << symbolName;
        if (error != nullptr) {
            stream << ": " << error;
        }
        stream << Stream::endl;
    }
    return symbol;
}

bool Initialize(DYN_CJNativeInterface* interf)
{
    auto anySym = reinterpret_cast<void*>(interf->stackGrowStub);

    Dl_info info;
    int code = dladdr(anySym, &info);
    if (code == 0) {
        Log::init.Stream(Logging::Level::ERROR) << "dladdr failed to find rt info" << Stream::endl;
        return false;
    }
    auto handle = Handle::Open(info.dli_fname);
    if (!handle.has_value()) {
        Log::init.Stream(Logging::Level::ERROR) << "failed to open lib " << info.dli_fname << Stream::endl;
        return false;
    }

    // verify that we didn't opened new library (any other exported symbol can be used).
    auto stackGrowStub = handle->Sym("CJ_MCC_StackGrowStub");
    Asm::engine_newthread_nret_function =
        handle->Func<decltype(Asm::engine_newthread_nret_function)>("CJ_MCC_NewCJThreadNoReturn");

    Asm::engine_read_generic = interf->readGenericField;
    WriteGeneric             = interf->writeGenericField;

    WriteStructField = interf->writeStructField;
    ReadStructField  = interf->readStructField;

    if (stackGrowStub != interf->stackGrowStub) {
        auto& stream = Log::init.Stream(Logging::Level::ERROR);
        stream << "incorrect stack grow stub address ";
        stream << interf->stackGrowStub << " " << stackGrowStub << Stream::endl;
        return false;
    }

#if defined(__APPLE__) && defined(STATIC_HELPER)
    if (interf->appLibHandle == nullptr) {
        Log::init.Stream(Logging::Level::ERROR)
            << "STATIC_HELPER requires a non-null application library handle" << Stream::endl;
        return false;
    }

    auto* thrower = LookupSymbol(
        interf->appLibHandle, "application", "_CN32cangjie.runtime.cbcengine.helper22throwImplicitExceptionHl"
    );
    auto* spawnFuture = LookupSymbol(interf->appLibHandle, "application", "helper_spawn_future");
#else
    #if defined(__APPLE__)
    const char* helperLibName = "@rpath/libcbcengine-helper.dylib";
    #else
    const char* helperLibName = "libcbcengine-helper.so";
    #endif
    auto helperHandleOpt      = Handle::Open(helperLibName);
    if (!helperHandleOpt.has_value()) {
        Log::init.Stream(Logging::Level::ERROR) << "failed to open lib " << helperLibName << Stream::endl;
        return false;
    }

    g_helperLibHandle = std::move(*helperHandleOpt);

    auto* thrower     = g_helperLibHandle.Sym("_CN32cangjie.runtime.cbcengine.helper22throwImplicitExceptionHl");
    auto* spawnFuture = g_helperLibHandle.Sym("helper_spawn_future");
#endif

    if (thrower == nullptr || spawnFuture == nullptr) {
        return false;
    }

    Asm::engine_implicit_exception_thrower = reinterpret_cast<void (*)(int)>(thrower);
    Asm::engine_spawn_future               = spawnFuture;
    return true;
}

} // namespace RTSupport
