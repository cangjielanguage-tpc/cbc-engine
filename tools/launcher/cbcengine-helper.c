#include <stddef.h>
#include <stdint.h>

// needed to enforce abi quirks (sret)
struct StackReturn {
    char data[32];
};

extern uintptr_t CJ_MCC_ReadRefField(void* object, uintptr_t* field);
extern void* CJ_MCC_NewCJThread(void* execute, void* future, void* scheduler);
extern struct StackReturn set_thread_handle(void* thread, void* handle, void* zero) __asm__(
    "_CNat6Thread24setRuntimeCJThreadHandleHPu"
);
extern void execute() __asm__("_CNat6FutureIG_E7executeHv");

struct Future {
    void* header;
    uintptr_t thread;
};

void helper_spawn_future(struct Future* future)
{
    void* handle     = CJ_MCC_NewCJThread((void*)&execute, future, NULL);
    uintptr_t thread = future->thread;
    if ((thread >> 48 != 0)) {
        thread = CJ_MCC_ReadRefField(future, &future->thread);
    }
    set_thread_handle((void*)thread, handle, NULL);
}
