#include "rt_syms.h"
#include "runtimesupport/impl/asm_trampolines.h"
#include "utils/logger.h"
#include "utils/ostream.h"
#include "utils/rt_logger.h"
#include <cstdint>
#include <dlfcn.h>
#include <optional>

namespace RTSupport {

void (*WriteStructField)(
    uintptr_t base, uintptr_t field, size_t fieldLen, uintptr_t src, size_t srcLen, DYN_GCTib gctib
);
void (*ReadStructField)(uintptr_t dst, uintptr_t base, uintptr_t field, size_t fieldLen, DYN_GCTib gctib);

void (*WriteGeneric)(uintptr_t base, uintptr_t field, uintptr_t obj, size_t size);

// merge with LibHandle
struct Handle {
    void* handle;
    std::string name;

    Handle(void* handle, std::string&& name) : handle(handle), name(std::move(name)) {}

    Handle(Handle const& handle) = delete;

    Handle(Handle&& handle) : handle(handle.handle) { handle.handle = nullptr; }

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

void Initialize(DYN_CJNativeInterface* interf)
{
    auto anySym = reinterpret_cast<void*>(interf->stackGrowStub);

    Dl_info info;
    int code = dladdr(anySym, &info);
    if (code == 0) {
        Log::init.Stream(Logging::Level::ERROR) << "dladdr failed to find rt info" << Stream::endl;
        return;
    }
    void* base  = info.dli_fbase;
    auto handle = Handle::Open(info.dli_fname);
    if (!handle.has_value()) {
        Log::init.Stream(Logging::Level::ERROR) << "failed to open lib " << info.dli_fname << Stream::endl;
        return;
    }

    // verify that we didn't opened new library (any other exported symbol can be used).
    auto stackGrowStub = handle->Sym("CJ_MCC_StackGrowStub");
    Asm::engine_newthread_nret_function =
        handle->Func<decltype(Asm::engine_newthread_nret_function)>("CJ_MCC_NewCJThreadNoReturn");

    Asm::engine_read_generic = handle->Func<decltype(Asm::engine_read_generic)>("CJ_MCC_ReadGeneric");
    WriteGeneric             = handle->Func<decltype(WriteGeneric)>("CJ_MCC_WriteGeneric");

    WriteStructField = handle->Func<decltype(WriteStructField)>("CJ_MCC_WriteStructField");
    ReadStructField  = handle->Func<decltype(ReadStructField)>("CJ_MCC_ReadStructField");

    if (stackGrowStub != interf->stackGrowStub) {
        auto& stream = Log::init.Stream(Logging::Level::ERROR);
        stream << "incorrect stack grow stub address ";
        stream << interf->stackGrowStub << " " << stackGrowStub << Stream::endl;
        return;
    }
}

} // namespace RTSupport
