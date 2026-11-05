#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>

namespace Cbc::Profiler {

#ifndef CBC_PROFILER_CALL_BUFFER_SIZE
    #define CBC_PROFILER_CALL_BUFFER_SIZE 4096
#endif

using FunctionId = uintptr_t;
using CallSiteId = uintptr_t;

class FiberProfile;
using FiberProfileHandle = std::shared_ptr<FiberProfile>;

class FiberProfileManager;
struct FiberProfileTestAccess;

class FiberProfile {
public:
    [[gnu::always_inline]] inline void RecordCall(FunctionId caller, CallSiteId callSite)
    {
        const auto writerIndex  = sharedWriterIndex & 1U;
        auto& current           = buffers[writerIndex];
        current.records[cursor] = CallRecord { caller, callSite };

        cursor = (cursor + 1) & (CALL_BUFFER_SIZE - 1);
        if (count < CALL_BUFFER_SIZE) {
            ++count;
        }

        if (cursor == 0) {
            TryPublish();
        }
    }

private:
    static constexpr uint32_t CALL_BUFFER_SIZE    = CBC_PROFILER_CALL_BUFFER_SIZE;
    static constexpr uint32_t NO_PUBLISHED_BUFFER = 2;

    struct CallRecord {
        FunctionId caller;
        CallSiteId callSite;
    };

    struct CallBuffer {
        std::array<CallRecord, CALL_BUFFER_SIZE> records;
        uint64_t recordCount { 0 };
    };

    // Producer-only. Called after filling the current buffer.
    [[gnu::noinline, gnu::cold]] void TryPublish();

    friend class FiberProfileManager;
    friend struct FiberProfileTestAccess;
    friend void DestroyFiberProfile(FiberProfileHandle& profile);

    std::array<CallBuffer, 2> buffers;

    // Producer-only state (not accessed by collector while active).
    uint32_t cursor { 0 }; // next physical slot in current buffer
    uint32_t count { 0 };  // valid entries in current buffer, capped at N

    std::mutex bufferMutex;
    uint32_t sharedWriterIndex { NO_PUBLISHED_BUFFER };

    std::atomic<uint64_t> recordCount { 0 };

    uint64_t processedCount { 0 };

    uint64_t observedCount { 0 };

    std::atomic<bool> active { true };
};

} // namespace Cbc::Profiler
