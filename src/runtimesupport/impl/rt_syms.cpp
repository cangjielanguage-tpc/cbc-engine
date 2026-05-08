#include "rt_syms.h"
#include "utils/logger.h"
#include "utils/ostream.h"
#include "utils/rt_logger.h"
#include <dlfcn.h>
#include <optional>

namespace RTSupport {

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

void Initialize(DYN_CJNativeInterfaceT* interf)
{
    auto anySym = reinterpret_cast<void*>(interf->arrayAlloc);

    Dl_info info;
    int code = dladdr(anySym, &info);
    if (code != 0) {
        Log::init.Stream(Logging::Level::ERROR) << "dladdr failed to find rt info" << Stream::endl;
        return;
    }
    void* base  = info.dli_fbase;
    auto handle = Handle::Open(info.dli_fname);
    if (!handle.has_value()) {
        Log::init.Stream(Logging::Level::ERROR) << "failed to open lib " << info.dli_fname << Stream::endl;
        return;
    }

    GetMTable = handle->Func<decltype(GetMTable)>("CJ_MCC_GetMTable");
}

} // namespace RTSupport
