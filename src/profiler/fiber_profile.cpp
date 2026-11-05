#include "fiber_profile.h"

namespace Cbc::Profiler {

void FiberProfile::TryPublish()
{
    const auto completedRecordCount =
        recordCount.fetch_add(CALL_BUFFER_SIZE, std::memory_order_relaxed) + CALL_BUFFER_SIZE;
    std::unique_lock<std::mutex> lock(bufferMutex, std::try_to_lock);
    if (!lock.owns_lock()) {
        return;
    }

    const auto currentWriterIndex           = sharedWriterIndex & 1U;
    buffers[currentWriterIndex].recordCount = completedRecordCount;
    cursor                                  = 0;
    count                                   = 0;
    sharedWriterIndex                       = currentWriterIndex ^ 1U;
}

} // namespace Cbc::Profiler
