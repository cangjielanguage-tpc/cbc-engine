#include "profiler/fiber_profile.h"

#include <cassert>
#include <cstdint>
#include <pthread.h>

#include "profiler/fiber_profile.cpp"

namespace Cbc::Profiler {

struct FiberProfileTestAccess {
    static constexpr uint32_t BufferSize() { return FiberProfile::CALL_BUFFER_SIZE; }

    static void ConsumePublished(FiberProfile& profile)
    {
        profile.ChangeReaderState(FiberProfile::ReaderState::INACTIVE, FiberProfile::ReaderState::ACTIVE);

        const auto writerIndex = profile.sharedWriterIndex.load();
        if (writerIndex != FiberProfile::NO_PUBLISHED_BUFFER) {
            const auto readerIndex = writerIndex ^ 1U;
            assert(readerIndex != (profile.sharedWriterIndex.load(std::memory_order_relaxed) & 1U));

            for (uint32_t i = 0; i < FiberProfile::CALL_BUFFER_SIZE; ++i) {
                const auto& record = profile.buffers[readerIndex].records[i];
                assert(record.caller != 0);
                assert(record.callSite != 0);
            }
        }

        profile.ChangeReaderState(FiberProfile::ReaderState::ACTIVE, FiberProfile::ReaderState::INACTIVE);
    }
};

} // namespace Cbc::Profiler

namespace {

using Cbc::Profiler::FiberProfile;
using Cbc::Profiler::FiberProfileTestAccess;

void* Producer(void* argument)
{
    auto& profile = *static_cast<FiberProfile*>(argument);
    for (uintptr_t batch = 1; batch <= 16; ++batch) {
        for (uintptr_t i = 0; i < FiberProfileTestAccess::BufferSize(); ++i) {
            const auto caller = batch * 100 + i;
            profile.RecordCall(caller, caller + 1000);
        }
    }
    return nullptr;
}

void* Consumer(void* argument)
{
    FiberProfileTestAccess::ConsumePublished(*static_cast<FiberProfile*>(argument));
    return nullptr;
}

} // namespace

int main()
{
    FiberProfile profile;
    pthread_t producer;
    pthread_t consumer;

    assert(pthread_create(&producer, nullptr, Producer, &profile) == 0);
    assert(pthread_create(&consumer, nullptr, Consumer, &profile) == 0);
    assert(pthread_join(producer, nullptr) == 0);
    assert(pthread_join(consumer, nullptr) == 0);
}
