#include <atomic>
#include <cstdint>
#include <threads.h>

#include "librace.h"
#include "model-assert.h"

namespace {

constexpr uint32_t BUFFER_SIZE = 1;
constexpr uint32_t NO_PUBLISHED_BUFFER = 2;

enum ReaderState : int {
    INACTIVE,
    ACTIVE,
    TRANSITION
};

uint32_t buffers[2][BUFFER_SIZE];
std::atomic<int> readerState;
std::atomic<uint32_t> sharedWriterIndex;

uint32_t writerIndex;
uint32_t cursor;

void TryPublish()
{
    auto previous = readerState.load();
    while (previous == TRANSITION || !readerState.compare_exchange_strong(previous, TRANSITION)) {
        if (previous == TRANSITION) {
            thrd_yield();
            previous = readerState.load();
        }
    }

    if (previous == INACTIVE) {
        writerIndex ^= 1U;
        cursor = 0;
        sharedWriterIndex.store(writerIndex);
    }

    readerState.store(previous);
}

void Producer(void*)
{
    for (uint32_t batch = 1; batch <= 3; ++batch) {
        for (uint32_t i = 0; i < BUFFER_SIZE; ++i) {
            store_32(&buffers[writerIndex][cursor], batch);
            cursor = (cursor + 1U) & (BUFFER_SIZE - 1U);
        }
        TryPublish();
    }
}

void Consumer(void*)
{
    int expected = INACTIVE;
    while (!readerState.compare_exchange_strong(expected, ACTIVE)) {
        expected = INACTIVE;
        thrd_yield();
    }

    const auto currentSharedWriterIndex = sharedWriterIndex.load();
    if (currentSharedWriterIndex != NO_PUBLISHED_BUFFER) {
        const auto readerIndex = currentSharedWriterIndex ^ 1U;
        MODEL_ASSERT(readerIndex != currentSharedWriterIndex);

        const auto currentWriterIndex = static_cast<uint32_t>(sharedWriterIndex.load());
        MODEL_ASSERT(readerIndex != currentWriterIndex);

        const auto record = load_32(&buffers[readerIndex][0]);

        MODEL_ASSERT(record != 0);
    }

    expected = ACTIVE;
    while (!readerState.compare_exchange_strong(expected, INACTIVE)) {
        expected = ACTIVE;
        thrd_yield();
    }
}

} // namespace

int user_main(int, char**)
{
    thrd_t producer;
    thrd_t consumer;

    readerState.store(INACTIVE);
    sharedWriterIndex.store(NO_PUBLISHED_BUFFER);
    writerIndex = 0;
    cursor = 0;

    thrd_create(&producer, reinterpret_cast<thrd_start_t>(&Producer), nullptr);
    thrd_create(&consumer, reinterpret_cast<thrd_start_t>(&Consumer), nullptr);
    thrd_join(producer);
    thrd_join(consumer);

    return 0;
}
