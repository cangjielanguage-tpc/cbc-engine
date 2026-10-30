#include "fiber_profile.h"

#include <thread>

namespace Cbc::Profiler {

void FiberProfile::TryPublish()
{
    const auto completedRecordCount =
        recordCount.fetch_add(CALL_BUFFER_SIZE, std::memory_order_relaxed) + CALL_BUFFER_SIZE;

    auto previous = readerState.load();
    while (previous == ReaderState::TRANSITION || !readerState.compare_exchange_weak(previous, ReaderState::TRANSITION)
    ) {
        if (previous == ReaderState::TRANSITION) {
            std::this_thread::yield();
            previous = readerState.load();
        }
    }

    if (previous == ReaderState::INACTIVE) {
        cursor = 0;
        count  = 0;

        const auto currentWriterIndex           = sharedWriterIndex.load(std::memory_order_relaxed) & 1U;
        buffers[currentWriterIndex].recordCount = completedRecordCount;
        sharedWriterIndex.store(currentWriterIndex ^ 1U);
    }

    readerState.store(previous);
}

void FiberProfile::ChangeReaderState(ReaderState from, ReaderState to)
{
    while (true) {
        auto expected = from;
        if (readerState.compare_exchange_weak(expected, to)) {
            return;
        }
        std::this_thread::yield();
    }
}

} // namespace Cbc::Profiler
