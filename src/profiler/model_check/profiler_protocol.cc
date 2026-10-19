#include <atomic>
#include <cstdint>
#include <threads.h>

#include "librace.h"
#include "model-assert.h"

namespace {

constexpr uint32_t BUFFER_SIZE = 1;
constexpr uint32_t NO_PUBLISHED_BUFFER = 2;

enum ReaderState : int {
    NO,
    YES,
    TRANS
};

uint32_t buffers[2][BUFFER_SIZE];
std::atomic<int> readerState;
std::atomic<uint32_t> sharedWriterIndex;

uint32_t writerIndex;
uint32_t cursor;

void TryPublish()
{
    auto previous = readerState.load(std::memory_order_relaxed);
    while (previous == TRANS || !readerState.compare_exchange_weak(
               previous, TRANS, std::memory_order_acq_rel, std::memory_order_relaxed
           )) {
        if (previous == TRANS) {
            thrd_yield();
            previous = readerState.load(std::memory_order_relaxed);
        }
    }

    if (previous == NO) {
        writerIndex ^= 1U;
        cursor = 0;
        sharedWriterIndex.store(writerIndex, std::memory_order_release);
    }

    readerState.store(previous, std::memory_order_release);
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
    int expected = NO;
    while (!readerState.compare_exchange_weak(
        expected, YES, std::memory_order_acquire, std::memory_order_relaxed
    )) {
        expected = NO;
        thrd_yield();
    }

    const auto currentSharedWriterIndex = sharedWriterIndex.load(std::memory_order_acquire);
    if (currentSharedWriterIndex != NO_PUBLISHED_BUFFER) {
        const auto readerIndex = currentSharedWriterIndex ^ 1U;
        MODEL_ASSERT(readerIndex != currentSharedWriterIndex);

        thrd_yield();

        const auto currentWriterIndex = static_cast<uint32_t>(
            sharedWriterIndex.load(std::memory_order_relaxed)
        );
        MODEL_ASSERT(readerIndex != currentWriterIndex);

        const auto record = load_32(&buffers[readerIndex][0]);

        MODEL_ASSERT(record != 0);
    }

    expected = YES;
    while (!readerState.compare_exchange_weak(
        expected, NO, std::memory_order_release, std::memory_order_relaxed
    )) {
        expected = YES;
        thrd_yield();
    }
}

} // namespace

int user_main(int, char**)
{
    thrd_t producer;
    thrd_t consumer;

    readerState.store(NO, std::memory_order_relaxed);
    sharedWriterIndex.store(NO_PUBLISHED_BUFFER, std::memory_order_relaxed);
    writerIndex = 0;
    cursor = 0;

    thrd_create(&producer, reinterpret_cast<thrd_start_t>(&Producer), nullptr);
    thrd_create(&consumer, reinterpret_cast<thrd_start_t>(&Consumer), nullptr);
    thrd_join(producer);
    thrd_join(consumer);

    return 0;
}
