#include "rt_syms.h"
#include "engine/rt_logger.h"
#include "utils/logger.h"
#include "utils/ostream.h"
#include <dlfcn.h>
#include <optional>

namespace RTSupport {

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

DYN_FuncPtrT* (*GetMTable)(DYN_TypeInfoT* t, DYN_TypeInfoT* itf);
void (*UpdateVMT)(DYN_TypeInfoT* t, DYN_TypeInfoT* itf, DYN_ExtensionDataT* extData);
DYN_TypeInfoT* (*GetMethodOuterTI)(DYN_TypeInfoT* t, DYN_TypeInfoT* itf, int index);

void Initialize(DYN_CJNativeInterfaceT* interf)
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

    // verify that we didn't opened new library.
    auto stackGrowStub = handle->Sym("CJ_MCC_StackGrowStub");
    if (stackGrowStub != interf->stackGrowStub) {
        auto& stream = Log::init.Stream(Logging::Level::ERROR);
        stream << "incorrect stack grow stub address ";
        stream << interf->stackGrowStub << " " << stackGrowStub << Stream::endl;
        return;
    }

    GetMTable = handle->Func<decltype(GetMTable)>("CJ_MCC_GetMTable");
    UpdateVMT = handle->Func<decltype(UpdateVMT)>("CJ_MCC_UpdateVMT");

    GetMethodOuterTI = handle->Func<decltype(GetMethodOuterTI)>("CJ_MCC_GetMethodOuterTI");
}

} // namespace RTSupport
