#include "sharedobj.h"
#include "utils/assertion.h"
#include <dlfcn.h>

namespace Utils {

SharedObject::SharedObject(SharedObject&& other) : handle(other.handle), name(std::move(other.name))
{
    other.handle = nullptr;
}

SharedObject::SharedObject() : handle(nullptr), name() {}

SharedObject::SharedObject(void* handle, std::string&& name) : handle(handle), name(std::move(name)) {}

SharedObject::~SharedObject()
{
    if (handle != nullptr) {
        dlclose(handle);
    }
}

SharedObject SharedObject::OpenCurrentExecutable()
{
    void* handle = dlopen(nullptr, RTLD_LAZY);
    return SharedObject(handle, "/proc/self/exe"); // not exactly "universal" name, but it's ok
}

SharedObject SharedObject::Open(std::string&& str)
{
    void* handle = dlopen(str.c_str(), RTLD_LAZY);
    return SharedObject(handle, std::move(str));
}

SharedObject SharedObject::Open(std::string_view str)
{
    std::string name(str);
    void* handle = dlopen(name.c_str(), RTLD_LAZY);
    return SharedObject(handle, std::move(name));
}

bool SharedObject::IsOpened() const { return handle != nullptr; }

SharedObject& SharedObject::operator=(SharedObject&& other)
{
    if (this == &other) {
        return *this;
    }
    if (handle != nullptr) {
        dlclose(this->handle);
    }
    this->handle = other.handle;
    this->name   = std::move(other.name);
    other.handle = nullptr;
    return *this;
}

void* SharedObject::SearchSym(char const* str) const
{
    if (handle) {
        return dlsym(handle, str);
    }
    return nullptr;
}

std::string const& SharedObject::Name() const { return name; }

} // namespace Utils
