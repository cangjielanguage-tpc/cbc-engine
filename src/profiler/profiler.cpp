#include "profiler.h"

#include "utils/ostream.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <list>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace Cbc::Profiler {

bool enabled = true;

Stream::Descripted Log::stream(Stream::cerr, "[profiler] ");
Logging::Logger Log::profiler(&Log::stream);

namespace {
constexpr uint64_t HOT_CALL_COUNT = 100;
constexpr auto COLLECTION_PERIOD  = std::chrono::seconds(1);
} // namespace

class FiberProfileManager {
public:
    static FiberProfileManager& Instance()
    {
        static FiberProfileManager instance;
        return instance;
    }

    FiberProfileHandle RegisterFiber()
    {
        auto profile = std::make_shared<FiberProfile>();
        std::list<FiberProfileHandle> one;
        one.emplace_back(profile);

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            pending.splice(pending.end(), one);
        }
        wakeup.notify_one();
        return profile;
    }

private:
    struct CallEdge {
        FunctionId caller;
        FunctionId callee;

        bool operator==(const CallEdge& other) const
        {
            return caller == other.caller && callee == other.callee;
        }
    };

    struct CallEdgeHash {
        size_t operator()(const CallEdge& edge) const
        {
            const auto a = std::hash<FunctionId> {}(edge.caller);
            const auto b = std::hash<FunctionId> {}(edge.callee);
            return a ^ (b << 1);
        }
    };

    FiberProfileManager() : worker(&FiberProfileManager::Run, this) {}

    ~FiberProfileManager()
    {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            stopping = true;
        }
        wakeup.notify_one();
        worker.join();
    }

    void ProcessRecord(const FiberProfile::CallRecord& record)
    {
        CallEdge edge { record.caller, record.callee };
        const auto count = ++counters[edge];
        if (count == HOT_CALL_COUNT) {
            Log::profiler.Log(Logging::Level::DEBUG, [&](Stream::Output& out) {
                out.PrintLn(
                    "hot call {} -> {}: {} calls",
                    reinterpret_cast<const void*>(edge.caller),
                    reinterpret_cast<const void*>(edge.callee),
                    count
                );
            });
        }
    }

    void ProcessBuffer(const FiberProfile::CallBuffer& buffer, uint32_t start, uint32_t count)
    {
        for (uint32_t i = 0; i < count; ++i) {
            const auto index = (start + i) & (FiberProfile::CALL_BUFFER_SIZE - 1);
            ProcessRecord(buffer.records[index]);
        }
    }

    void ConsumePublished(FiberProfile& profile)
    {
        for (auto& buffer : profile.buffers) {
            if (!buffer.ready.load(std::memory_order_acquire)) {
                continue;
            }

            ProcessBuffer(buffer, 0, FiberProfile::CALL_BUFFER_SIZE);
            buffer.ready.store(false, std::memory_order_release);
        }
    }

    bool ConsumeProfile(FiberProfile& profile)
    {
        // Take one snapshot before reading ready flags. If the producer has
        // finished, this acquire makes all of its final state visible.
        const bool finished = !profile.active.load(std::memory_order_acquire);

        ConsumePublished(profile);

        if (finished) {
            // Producer is permanently gone, so it is now safe to read
            // even the never-published, partially-filled active buffer.
            auto& current        = profile.buffers[profile.writerIndex];
            const uint32_t start =
                profile.count == FiberProfile::CALL_BUFFER_SIZE ? profile.cursor : 0;
            ProcessBuffer(current, start, profile.count);
        }

        return finished;
    }

    void ConsumeActiveProfiles()
    {
        for (auto it = activeProfiles.begin(); it != activeProfiles.end();) {
            auto& profile = **it;
            if (ConsumeProfile(profile)) {
                it = activeProfiles.erase(it);
            } else {
                ++it;
            }
        }
    }

    void Run()
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        while (true) {
            wakeup.wait_for(lock, COLLECTION_PERIOD, [&] { return stopping || !pending.empty(); });

            std::list<FiberProfileHandle> newcomers;
            newcomers.splice(newcomers.end(), pending);
            const bool shouldStop = stopping;
            lock.unlock();

            activeProfiles.splice(activeProfiles.end(), newcomers);
            ConsumeActiveProfiles();

            if (shouldStop) {
                return;
            }
            lock.lock();
        }
    }

    std::mutex queueMutex;
    std::condition_variable wakeup;
    std::list<FiberProfileHandle> pending;
    bool stopping { false }; // guarded by queueMutex

    // Owned exclusively by collector thread.
    std::list<FiberProfileHandle> activeProfiles;
    std::unordered_map<CallEdge, uint64_t, CallEdgeHash> counters;

    std::thread worker;
};

void FiberProfile::TryPublish()
{
    const uint32_t nextIndex = writerIndex ^ 1u;
    auto& next               = buffers[nextIndex];

    if (next.ready.load(std::memory_order_acquire)) {
        return;
    }

    auto& current = buffers[writerIndex];
    current.ready.store(true, std::memory_order_release);

    writerIndex = nextIndex;
    cursor      = 0;
    count       = 0;
}

FiberProfileHandle CreateFiberProfile()
{
    return FiberProfileManager::Instance().RegisterFiber();
}

void DestroyFiberProfile(FiberProfileHandle& profile)
{
    if (profile) {
        profile->active.store(false, std::memory_order_release);
        profile.reset();
    }
}

} // namespace Cbc::Profiler
