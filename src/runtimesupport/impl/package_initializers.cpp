#include "cbc_engine.h"

#if defined(TARGET_OS_IOS) && TARGET_OS_IOS

#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <mach-o/getsect.h>

#include <cstdint>
#include <cstring>
#include <string_view>

#include "cj-interface.h"
#include "utils/rt_logger.h"

static int RunPackageInitializers()
{
    const auto* header = _dyld_get_image_header(0);
    if (header == nullptr || header->magic != MH_MAGIC_64) {
        LOG_ERROR(RTSupport::Log::rt, "could not read the main executable Mach-O header");
        return E_FAILED;
    }
    unsigned long size = 0;
    const auto* globalInitializersData = getsectiondata(
        reinterpret_cast<const mach_header_64*>(header), "__CJ_METADATA", "__cjglobalFunc", &size);
    if (globalInitializersData == nullptr) {
        LOG_DEBUG(RTSupport::Log::rt, "no package initializer section in the main executable");
        return E_OK;
    }

    using TaskFunc = void* (*)(void*);
    auto runTask = reinterpret_cast<void* (*)(TaskFunc, void*)>(dlsym(RTLD_DEFAULT, "RunCJTask"));
    auto getTaskResult = reinterpret_cast<int (*)(void*, void**)>(dlsym(RTLD_DEFAULT, "GetTaskRet"));
    auto releaseTask = reinterpret_cast<void (*)(void*)>(dlsym(RTLD_DEFAULT, "ReleaseHandle"));
    if (runTask == nullptr || getTaskResult == nullptr || releaseTask == nullptr) {
        LOG_ERROR(RTSupport::Log::rt, "could not resolve the runtime task APIs for package initialization");
        return E_FAILED;
    }

    // Each record is a relocated function address and a NUL-terminated name, padded to eight bytes.
    constexpr std::size_t alignment = 8;
    for (std::size_t offset = 0; offset < size;) {
        if (size - offset <= sizeof(uintptr_t)) {
            LOG_ERROR(RTSupport::Log::rt, "truncated package initializer record at offset {}", offset);
            return E_FAILED;
        }
        uintptr_t address;
        std::memcpy(&address, globalInitializersData + offset, sizeof(address));
        const auto* name = reinterpret_cast<const char*>(globalInitializersData + offset + sizeof(address));
        const auto* end = static_cast<const char*>(std::memchr(name, '\0', size - offset - sizeof(address)));
        if (end == nullptr) {
            LOG_ERROR(RTSupport::Log::rt, "unterminated package initializer name at offset {}", offset);
            return E_FAILED;
        }
        const auto nameLength = static_cast<std::size_t>(end - name);
        const auto recordEnd = offset + sizeof(address) + nameLength + 1;
        const auto padding = (alignment - recordEnd % alignment) % alignment;
        offset = padding <= size - recordEnd ? recordEnd + padding : size;

        if (address == 0 || std::string_view(name, nameLength).find("_CGP") == std::string_view::npos) {
            continue;
        }
        LOG_TRACE(RTSupport::Log::rt, "running package initializer {}", name);
        // RunCJTask enters the Cangjie calling convention through the runtime's task stub.
        auto task = runTask(reinterpret_cast<TaskFunc>(address), nullptr);
        if (task == nullptr) {
            LOG_ERROR(RTSupport::Log::rt, "could not start package initializer {}", name);
            return E_FAILED;
        }
        void* unusedResult = nullptr;
        const auto status = getTaskResult(task, &unusedResult);
        releaseTask(task);
        if (status != 0) {
            LOG_ERROR(RTSupport::Log::rt, "package initializer {} failed with status {}", name, status);
            return status;
        }
    }
    return E_OK;
}

extern "C" CBC_EXPORT int init_cbc_interpreter(const InterpreterParam* param)
{
    auto init = reinterpret_cast<RTErrorCode (*)(const InterpreterParam*)>(
        dlsym(RTLD_DEFAULT, "InitCJInterpreter"));
    if (init == nullptr) {
        LOG_ERROR(RTSupport::Log::rt, "could not resolve InitCJInterpreter");
        return E_FAILED;
    }

    const auto status = init(param);
    if (status != E_OK) {
        LOG_ERROR(RTSupport::Log::rt, "InitCJInterpreter failed with status {}", static_cast<int>(status));
        return status;
    }
    return RunPackageInitializers();
}

#endif
