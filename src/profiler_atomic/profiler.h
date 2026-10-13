#pragma once

#include "utils/logger.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>

namespace Cbc::Profiler {

using FunctionId = uintptr_t;
using CallSiteId = uintptr_t;

// Do not modify concurrently without additional synchronization.
extern bool enabled;

class FiberProfile;
using FiberProfileHandle = std::shared_ptr<FiberProfile>;

class FiberProfileManager;

class FiberProfile {
public:
    [[gnu::always_inline]] inline void RecordCall(FunctionId caller, CallSiteId callSite)
    {
        const uint64_t write = writeSequence;

        const uint64_t read = readSequence.load(std::memory_order_acquire);
        if (write - read >= CALL_BUFFER_SIZE) {
            ++droppedCalls;
            return;
        }

        records[write & (CALL_BUFFER_SIZE - 1)] = CallRecord { caller, callSite };
        writeSequence                           = write + 1;

        publishedSequence.store(write + 1, std::memory_order_release);
    }

private:
    static constexpr uint64_t CALL_BUFFER_SIZE = 8192;
    static_assert(CALL_BUFFER_SIZE > 0 && (CALL_BUFFER_SIZE & (CALL_BUFFER_SIZE - 1)) == 0);

    struct CallRecord {
        FunctionId caller;
        CallSiteId callSite;
    };

    friend class FiberProfileManager;
    friend void DestroyFiberProfile(FiberProfileHandle& profile);

    std::array<CallRecord, CALL_BUFFER_SIZE> records;

    uint64_t writeSequence { 0 };
    uint64_t droppedCalls { 0 };

    std::atomic<uint64_t> publishedSequence { 0 };

    std::atomic<uint64_t> readSequence { 0 };

    std::atomic<bool> active { true };
};

namespace Log {
extern Stream::Descripted stream;
extern Logging::Logger profiler;
} // namespace Log

FiberProfileHandle CreateFiberProfile();

void DestroyFiberProfile(FiberProfileHandle& profile);

} // namespace Cbc::Profiler
