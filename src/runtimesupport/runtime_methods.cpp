#include "runtime_methods.h"

namespace RTMethods {

static getMTable_t g_getMTable;

getMTable_t GetMTable() { return g_getMTable; }

void Init()
{
#if defined(_WIN32) || defined(_WIN64)
    auto libName = "cangjie-runtime.dll";
#elif defined(__APPLE__)
    auto libName = "libcangjie-runtime.dylib";
#else
    auto libName = "libcangjie-runtime.so";
#endif

    void* libHandle = dlopen(libName, RTLD_LAZY);

    g_getMTable = reinterpret_cast<getMTable_t>(dlsym(libHandle, "CJ_MCC_GetMTable"));

    dlclose(libHandle);
};

} // namespace RTMethods
