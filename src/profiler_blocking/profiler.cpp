#include "profiler.h"

#include "cbc/isa_rt.h"
#include "interpreter/function_handle.h"
#include "utils/assertion.h"
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
        auto profile = FiberProfileHandle(new FiberProfile(*this));
        std::list<FiberProfileHandle> one;
        one.emplace_back(profile);

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            pending.splice(pending.end(), one);
        }
        wakeup.notify_one();
        return profile;
    }

    void NotifyCollector()
    {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            workPending = true;
        }
        wakeup.notify_one();
    }

private:
    struct CallEdge {
        FunctionId caller;
        CallSiteId callSite;

        bool operator==(const CallEdge& other) const { return caller == other.caller && callSite == other.callSite; }
    };

    struct CallEdgeHash {
        size_t operator()(const CallEdge& edge) const
        {
            const auto a = std::hash<FunctionId> {}(edge.caller);
            const auto b = std::hash<CallSiteId> {}(edge.callSite);
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
        const CallEdge edge { record.caller, record.callSite };
        const auto count = ++counters[edge];
        if (count == HOT_CALL_COUNT) {
            auto* callerFuh = reinterpret_cast<Interpretation::DynamicFunctionHandle*>(edge.caller);
            auto* bytecode  = callerFuh->bytecode.load(std::memory_order_relaxed);
            ASSERT(bytecode != nullptr);

            const auto bytecodeStart = reinterpret_cast<uintptr_t>(bytecode->code.bytecode);
            ASSERT(bytecodeStart <= edge.callSite);
            ASSERT(edge.callSite < bytecodeStart + bytecode->code.bytecodeSize);
            const auto callSiteOffset = edge.callSite - bytecodeStart;

            auto* callSite = reinterpret_cast<uint8_t*>(edge.callSite);
            Decoder::ByteReader reader(
                callSite, bytecode->code.bytecode, bytecode->code.bytecode + bytecode->code.bytecodeSize
            );
            const auto args = Cbc::RT::B3xi12::Decode(reader);
            ASSERT(args.opc == Cbc::RT::Opcode::DIRECT_CALL_2I);
            const auto callee = bytecode->code.literals->at(args.xi12.imm12).uintptr;

            Log::profiler.Log(Logging::Level::DEBUG, [&](Stream::Output& out) {
                out.PrintLn(
                    "hot call {}+{} -> {}: {} calls",
                    reinterpret_cast<const void*>(edge.caller),
                    callSiteOffset,
                    reinterpret_cast<const void*>(callee),
                    count
                );
            });
        }
    }

    void ProcessPublished(FiberProfile& profile)
    {
        for (auto& buffer : profile.buffers) {
            if (!buffer.ready.load(std::memory_order_acquire)) {
                continue;
            }

            for (const auto& record : buffer.records) {
                ProcessRecord(record);
            }
            {
                std::lock_guard<std::mutex> lock(profile.bufferMutex);
                buffer.ready.store(false, std::memory_order_release);
            }
            profile.bufferAvailable.notify_one();
        }
    }

    bool ConsumeProfile(FiberProfile& profile)
    {
        const bool finished = !profile.active.load(std::memory_order_acquire);

        ProcessPublished(profile);

        if (finished) {
            std::lock_guard<std::mutex> lock(profile.bufferMutex);
            const auto& current = profile.buffers[profile.writerIndex];
            for (uint32_t i = 0; i < profile.cursor; ++i) {
                ProcessRecord(current.records[i]);
            }
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
            wakeup.wait_for(lock, COLLECTION_PERIOD, [&] { return stopping || workPending || !pending.empty(); });

            std::list<FiberProfileHandle> newcomers;
            newcomers.splice(newcomers.end(), pending);
            const bool shouldStop = stopping;
            workPending           = false;
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
    bool stopping { false };    // guarded by queueMutex
    bool workPending { false }; // guarded by queueMutex

    // Owned exclusively by collector thread.
    std::list<FiberProfileHandle> activeProfiles;
    std::unordered_map<CallEdge, uint64_t, CallEdgeHash> counters;

    std::thread worker;
};

void FiberProfile::PublishAndWaitForNext()
{
    std::unique_lock<std::mutex> lock(bufferMutex);
    const uint32_t nextIndex = writerIndex ^ 1u;
    auto& next               = buffers[nextIndex];

    bufferAvailable.wait(lock, [&] { return !next.ready.load(std::memory_order_acquire); });

    buffers[writerIndex].ready.store(true, std::memory_order_release);
    writerIndex = nextIndex;
    cursor      = 0;
    lock.unlock();

    manager.NotifyCollector();
}

FiberProfileHandle CreateFiberProfile() { return FiberProfileManager::Instance().RegisterFiber(); }

void DestroyFiberProfile(FiberProfileHandle& profile)
{
    if (profile) {
        profile->active.store(false, std::memory_order_release);
        FiberProfileManager::Instance().NotifyCollector();
        profile.reset();
    }
}

} // namespace Cbc::Profiler
