#include "asm_trampolines.h"
#include "cjnative.h"

#include <type_traits>

namespace {

struct FutureObject {
    void* header;
    DYN_ObjRef thread;
};

struct SpawnFutureResult {
    DYN_ObjRef thread;
    DYN_CJThreadHandle handle;
};

static_assert(std::is_standard_layout_v<SpawnFutureResult>);
static_assert(std::is_trivially_copyable_v<SpawnFutureResult>);
static_assert(sizeof(SpawnFutureResult) == 2 * sizeof(void*));
static_assert(alignof(SpawnFutureResult) == alignof(void*));

} // namespace

extern "C" __attribute__((visibility("hidden"))) SpawnFutureResult engine_spawn_future(DYN_ObjRef future)
{
    auto handle = g_CJNativeInterfaceInstance.newCJThread(Asm::engine_future_execute_function, future, nullptr);

    auto* futureObject = static_cast<FutureObject*>(future);
    auto thread        = g_CJNativeInterfaceInstance.readInstanceField(future, &futureObject->thread);
    return { thread, handle };
}
