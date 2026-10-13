#pragma once

#include "utils/logger.h"

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>

namespace Cbc::Profiler {

using FunctionId = uintptr_t;
using CallSiteId = uintptr_t;

extern bool enabled;

class FiberProfile;
using FiberProfileHandle = std::shared_ptr<FiberProfile>;

class FiberProfileManager;

class FiberProfile {
public:
    [[gnu::always_inline]] inline void RecordCall(FunctionId caller, CallSiteId callSite)
    {
        buffers[writerIndex].records[cursor++] = CallRecord { caller, callSite };
        if (cursor == CALL_BUFFER_SIZE) {
            PublishAndWaitForNext();
        }
    }

private:
    static constexpr uint32_t CALL_BUFFER_SIZE = 4096;

    struct CallRecord {
        FunctionId caller;
        CallSiteId callSite;
    };

    struct CallBuffer {
        std::array<CallRecord, CALL_BUFFER_SIZE> records;
        std::atomic<bool> ready { false };
    };

    explicit FiberProfile(FiberProfileManager& manager) : manager(manager) {}

    [[gnu::noinline, gnu::cold]] void PublishAndWaitForNext();

    friend class FiberProfileManager;
    friend void DestroyFiberProfile(FiberProfileHandle& profile);

    FiberProfileManager& manager;
    std::array<CallBuffer, 2> buffers;

    uint32_t writerIndex { 0 };
    uint32_t cursor { 0 };

    std::mutex bufferMutex;
    std::condition_variable bufferAvailable;

    std::atomic<bool> active { true };
};

namespace Log {
extern Stream::Descripted stream;
extern Logging::Logger profiler;
} // namespace Log

FiberProfileHandle CreateFiberProfile();

void DestroyFiberProfile(FiberProfileHandle& profile);

} // namespace Cbc::Profiler
