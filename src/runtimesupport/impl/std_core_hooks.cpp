// std.core @C externs that have no CHIR body in std.core.chir: the AOT
// libcangjie-std-core.so provides them, but when std.core itself is compiled
// to CBC no AOT std library is loaded, so the engine provides the
// implementations instead. They are resolved through the foreign-libs search,
// which includes the engine's own image (see Loader::Build).

#include "cbc_engine.h"

#include <atomic>

namespace {

// Spinlock guarding the std.core atexit callback list. Mirrors the AOT
// implementation: a one-byte test-and-set exchange loop. "Unlook" mirrors the
// upstream symbol spelling (the schema typo is intentional, see CHIR.scala).
std::atomic<uint8_t> g_atExitCallbackListLocked{0};

} // namespace

extern "C" {

CBC_EXPORT void CJ_CORE_AtExitCallbackListLock()
{
    auto expected = uint8_t{0};
    while (!g_atExitCallbackListLocked.compare_exchange_weak(
        expected, 1, std::memory_order_acquire, std::memory_order_relaxed)) {
        expected = 0;
    }
}

CBC_EXPORT void CJ_CORE_AtExitCallbackListUnlook()
{
    g_atExitCallbackListLocked.store(0, std::memory_order_release);
}

} // extern "C"
