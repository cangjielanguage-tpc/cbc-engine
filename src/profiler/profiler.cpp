#include "profiler.h"

#include "utils/ostream.h"

#include "utils/assertion.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace Cbc::Profiler {

bool enabled = true;

Stream::Descripted Log::stream(Stream::cerr, "[profiler] ");
Logging::Logger Log::profiler(&Log::stream);

namespace {

using namespace std::chrono_literals;

constexpr auto COLLECTION_PERIOD = 1s;

struct ThreadProfile {
    uint64_t writeSequence { 0 }; // interpreter thread only
    uint64_t readSequence { 0 };  // collector thread only
    std::array<CallRecord, CALL_BUFFER_SIZE> records;
    std::atomic<bool> active { true };
};

struct CallEdge {
    FunctionId caller;
    FunctionId callee;

    bool operator==(const CallEdge& other) const { return caller == other.caller && callee == other.callee; }
};

struct CallEdgeHash {
    size_t operator()(const CallEdge& edge) const
    {
        auto callerHash = std::hash<FunctionId> {}(edge.caller);
        auto calleeHash = std::hash<FunctionId> {}(edge.callee);
        return callerHash ^ (calleeHash << 1);
    }
};

class Manager {
public:
    static Manager& Instance()
    {
        static Manager manager;
        return manager;
    }

    std::shared_ptr<ThreadProfile> RegisterThread()
    {
        auto profile = std::make_shared<ThreadProfile>();
        std::lock_guard<std::mutex> lock(profilesMutex);
        profiles.emplace_back(profile);
        return profile;
    }

private:
    Manager() : worker(&Manager::Run, this) {}

    ~Manager()
    {
        {
            std::lock_guard<std::mutex> lock(wakeupMutex);
            stopping = true;
        }
        wakeup.notify_one();
        worker.join();
    }

    void Run()
    {
        std::unique_lock<std::mutex> lock(wakeupMutex);
        while (!wakeup.wait_for(lock, COLLECTION_PERIOD, [&] { return stopping; })) {
            lock.unlock();
            Collect();
            lock.lock();
        }
        lock.unlock();
        Collect();
    }

    void Collect()
    {
        auto collection  = ConsumeBuffers();
        auto collected   = collection.first;
        auto overwritten = collection.second;
        if (collected == 0 && overwritten == 0) {
            return;
        }

        Log::profiler.Log(Logging::Level::DEBUG, [&](Stream::Output& out) {
            out.PrintLn(
                "collected {} calls, overwritten {} calls, {} edges total", collected, overwritten, counters.size()
            );
        });
    }

    std::pair<uint64_t, uint64_t> ConsumeBuffers()
    {
        std::lock_guard<std::mutex> lock(profilesMutex);

        uint64_t collected   = 0;
        uint64_t overwritten = 0;
        for (const auto& profile : profiles) {
            while (true) {
                auto expectedSequence = profile->readSequence;
                const auto& record    = profile->records[expectedSequence & (CALL_BUFFER_SIZE - 1)];

                auto versionBefore = record.version.load(std::memory_order_acquire);
                if (versionBefore == 0 || (versionBefore & 1) != 0) {
                    break;
                }

                auto recordSequence = versionBefore / 2 - 1;
                if (recordSequence < expectedSequence) {
                    break;
                }
                if (recordSequence > expectedSequence) {
                    // This slot has wrapped, but another slot can still hold
                    // an older record. Find the oldest complete record instead
                    // of unnecessarily discarding the rest of the ring.
                    auto oldestSequence = recordSequence;
                    for (const auto& candidate : profile->records) {
                        auto candidateVersion = candidate.version.load(std::memory_order_acquire);
                        if (candidateVersion == 0 || (candidateVersion & 1) != 0) {
                            continue;
                        }
                        auto candidateSequence = candidateVersion / 2 - 1;
                        if (candidateSequence >= expectedSequence && candidateSequence < oldestSequence) {
                            oldestSequence = candidateSequence;
                        }
                    }
                    overwritten           += oldestSequence - expectedSequence;
                    profile->readSequence  = oldestSequence;
                    continue;
                }

                auto caller       = record.caller.load(std::memory_order_relaxed);
                auto callee       = record.callee.load(std::memory_order_relaxed);
                auto versionAfter = record.version.load(std::memory_order_acquire);
                if (versionBefore != versionAfter) {
                    continue;
                }

                CallEdge edge { caller, callee };
                auto& count = counters[edge];
                count++;
                collected++;
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
                profile->readSequence = recordSequence + 1;
            }
        }

        profiles.erase(
            std::remove_if(
                profiles.begin(),
                profiles.end(),
                [](const auto& profile) {
                    return !profile->active.load(std::memory_order_acquire) &&
                           profile->readSequence == profile->writeSequence;
                }
            ),
            profiles.end()
        );

        return { collected, overwritten };
    }

    std::mutex profilesMutex;
    std::vector<std::shared_ptr<ThreadProfile>> profiles;

    std::unordered_map<CallEdge, uint64_t, CallEdgeHash> counters;

    std::mutex wakeupMutex;
    std::condition_variable wakeup;
    bool stopping { false };
    std::thread worker;
};

class ThreadRegistration {
public:
    ~ThreadRegistration()
    {
        if (profile) {
            profile->active.store(false, std::memory_order_release);
        }
    }

    std::shared_ptr<ThreadProfile> profile;
};

thread_local ThreadRegistration threadRegistration;
thread_local ThreadProfile* currentProfile = nullptr;

} // namespace

void RegisterCurrentThread()
{
    if (currentProfile != nullptr) {
        return;
    }
    threadRegistration.profile = Manager::Instance().RegisterThread();
    currentProfile             = threadRegistration.profile.get();
}

void RecordCall(FunctionId caller, FunctionId callee)
{
    ASSERT(currentProfile != nullptr);
    auto& profile          = *currentProfile;
    auto sequence          = profile.writeSequence++;
    auto& record           = profile.records[sequence & (CALL_BUFFER_SIZE - 1)];
    auto inProgressVersion = sequence * 2 + 1;

    record.version.store(inProgressVersion, std::memory_order_relaxed);
    // Make the in-progress marker visible before either payload word. This is
    // a compiler barrier on x86_64 and a store barrier on AArch64.
    std::atomic_thread_fence(std::memory_order_release);
    record.caller.store(caller, std::memory_order_relaxed);
    record.callee.store(callee, std::memory_order_relaxed);
    record.version.store(inProgressVersion + 1, std::memory_order_release);
}
} // namespace Cbc::Profiler
