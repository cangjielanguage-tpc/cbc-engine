#pragma once

#include "utils/logger.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>

namespace Cbc::Profiler {

using FunctionId = uintptr_t;

extern bool enabled;

class FiberProfile;
using FiberProfileHandle = std::shared_ptr<FiberProfile>;

class FiberProfileManager;

class FiberProfile {
public:
    inline void RecordCall(FunctionId caller, FunctionId callee)
    {
        auto& current = buffers[writerIndex];
        current.records[cursor] = CallRecord { caller, callee };

        cursor = (cursor + 1) & (CALL_BUFFER_SIZE - 1);
        if (count < CALL_BUFFER_SIZE) {
            ++count;
        }

        if (cursor == 0) {
            TryPublish();
        }
    }

private:
    static constexpr uint32_t CALL_BUFFER_SIZE = 4096;

    struct CallRecord {
        FunctionId caller;
        FunctionId callee;
    };

    struct CallBuffer {
        std::array<CallRecord, CALL_BUFFER_SIZE> records;
        std::atomic<bool> ready { false };
    };

    // Producer-only. Called after filling the current buffer. If the collector
    // still owns the other buffer, keep the current one and overwrite it in place.
    void TryPublish();

    friend class FiberProfileManager;
    friend void DestroyFiberProfile(FiberProfileHandle& profile);

    std::array<CallBuffer, 2> buffers;

    // Producer-only state (not accessed by collector while active).
    uint32_t writerIndex { 0 };
    uint32_t cursor { 0 }; // next physical slot in current buffer
    uint32_t count { 0 };  // valid entries in current buffer, capped at N

    std::atomic<bool> active { true };
};

namespace Log {
extern Stream::Descripted stream;
extern Logging::Logger profiler;
} // namespace Log

FiberProfileHandle CreateFiberProfile();

// Call only after the last RecordCall on this fiber has completed.
void DestroyFiberProfile(FiberProfileHandle& profile);

} // namespace Cbc::Profiler
