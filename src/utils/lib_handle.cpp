#include <dlfcn.h>
#include <utility>

#include "assertion.h"
#include "lib_handle.h"

LibHandle::LibHandle(std::string libName) : handle(nullptr)
{
    handle = dlopen(libName.c_str(), RTLD_LAZY);
    if (!handle) {
        ASSERTION(false, dlerror());
    }
}

LibHandle::~LibHandle()
{
    if (handle) {
        dlclose(handle);
    }
}

LibHandle::LibHandle(LibHandle&& other) noexcept : handle(std::exchange(other.handle, nullptr)) {}

AotCodeAddr LibHandle::FindTarget(std::string linkageName) const { return dlsym(handle, linkageName.c_str()); }
