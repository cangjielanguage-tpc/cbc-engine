#include "fiber_profile.h"

#include <thread>

namespace Cbc::Profiler {

void FiberProfile::TryPublish()
{
    auto previous = readerState.load();
    while (previous == ReaderState::TRANSITION ||
           !readerState.compare_exchange_weak(previous, ReaderState::TRANSITION)) {
        if (previous == ReaderState::TRANSITION) {
            std::this_thread::yield();
            previous = readerState.load();
        }
    }

    if (previous == ReaderState::INACTIVE) {
        cursor = 0;
        count  = 0;

        const auto writerIndex = (sharedWriterIndex.load(std::memory_order_relaxed) & 1U) ^ 1U;
        sharedWriterIndex.store(writerIndex);
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
