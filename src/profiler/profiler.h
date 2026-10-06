#pragma once

#include "utils/logger.h"

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace Cbc::Profiler {

using FunctionId = uintptr_t;

extern bool enabled;

struct CallRecord {
    std::atomic<FunctionId> caller { 0 };
    std::atomic<FunctionId> callee { 0 };
    std::atomic<uint64_t> version { 0 };
};

constexpr size_t CALL_BUFFER_SIZE = 4096; // need to be pow of 2
constexpr uint64_t HOT_CALL_COUNT = 100;

namespace Log {
extern Stream::Descripted stream;
extern Logging::Logger profiler;
} // namespace Log

// Register the current thread's fixed-size buffer.
void RegisterCurrentThread();

// Requires RegisterCurrentThread() to have been called on the current thread.
void RecordCall(FunctionId caller, FunctionId callee);

} // namespace Cbc::Profiler
