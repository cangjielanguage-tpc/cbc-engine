#include "profiler.h"

#include "cbc/isa_rt.h"
#include "engine/decode/reader.h"
#include "interpreter/function_handle.h"
#include "profiler/profiler_logger.h"
#include "utils/assertion.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <list>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace Cbc::Profiler {

bool enabled = true;

namespace {
constexpr auto COLLECTION_PERIOD = std::chrono::milliseconds(10);
constexpr uint64_t HOT_CALL_COUNT = 100;
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
        CallSiteId callSite;

        bool operator==(const CallEdge& other) const { return caller == other.caller && callSite == other.callSite; }
    };

    struct CallEdgeHash {
        size_t operator()(const CallEdge& edge) const
        {
            const auto callerHash   = std::hash<FunctionId> {}(edge.caller);
            const auto callSiteHash = std::hash<CallSiteId> {}(edge.callSite);
            return callerHash ^ (callSiteHash << 1);
        }
    };

    using CallCounters = std::unordered_map<CallEdge, uint64_t, CallEdgeHash>;

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

    static std::string ResolveFunctionName(
        Engine::Session& session, const Interpretation::DynamicFunctionHandle& function
    )
    {
        const auto definition = Decode::Read(session, function.methodDef);
        const auto typeName   = Decode::Read(session, definition.TypeName());
        const auto methodName = Decode::Read(session, definition.Name());

        std::string result;
        result.reserve(typeName.size() + 1 + methodName.size());
        result.append(typeName.data(), typeName.size());
        result.push_back('.');
        result.append(methodName.data(), methodName.size());
        return result;
    }

    ProfiledCall ResolveCall(Engine::Session& session, const CallEdge& edge, uint64_t count) const
    {
        auto* callerFuh = reinterpret_cast<Interpretation::DynamicFunctionHandle*>(edge.caller);
        auto* bytecode  = callerFuh->bytecode.load(std::memory_order_relaxed);
        ASSERT(bytecode != nullptr);

        const auto bytecodeStart = reinterpret_cast<uintptr_t>(bytecode->code.bytecode);
        ASSERT(bytecodeStart <= edge.callSite);
        ASSERT(edge.callSite < bytecodeStart + bytecode->code.bytecodeSize);

        auto* callSite = reinterpret_cast<uint8_t*>(edge.callSite);
        Decoder::ByteReader reader(
            callSite, bytecode->code.bytecode, bytecode->code.bytecode + bytecode->code.bytecodeSize
        );
        const auto args = Cbc::RT::B3xi12::Decode(reader);
        ASSERT(args.opc == Cbc::RT::Opcode::DIRECT_CALL_2I);
        const auto callee = bytecode->code.literals->at(args.xi12.imm12).uintptr;
        auto* calleeFuh   = reinterpret_cast<Interpretation::DynamicFunctionHandle*>(callee);

        return {
            edge.caller,
            ResolveFunctionName(session, *callerFuh),
            edge.callSite - bytecodeStart,
            callee,
            ResolveFunctionName(session, *calleeFuh),
            count,
            count >= HOT_CALL_COUNT,
        };
    }

    void ProcessCollectedCalls(const CallCounters& counters)
    {
        Engine::Session session(Engine::GetEngineInstance());
        std::vector<ProfiledCall> calls;
        calls.reserve(counters.size());
        for (const auto& [edge, count] : counters) {
            calls.emplace_back(ResolveCall(session, edge, count));
        }
        LogProfilerPass(calls);
    }

    void ProcessBuffer(const FiberProfile::CallBuffer& buffer, uint32_t start, uint32_t count, CallCounters& counters)
    {
        for (uint32_t i = 0; i < count; ++i) {
            const auto index = (start + i) & (FiberProfile::CALL_BUFFER_SIZE - 1);
            const auto& record = buffer.records[index];
            ++counters[CallEdge { record.caller, record.callSite }];
        }
    }

    static void ChangeReaderState(
        FiberProfile& profile,
        FiberProfile::ReaderState from,
        FiberProfile::ReaderState to,
        std::memory_order order
    )
    {
        while (true) {
            auto expected = from;
            if (profile.readerState.compare_exchange_weak(expected, to, order, std::memory_order_relaxed)) {
                return;
            }
            std::this_thread::yield();
        }
    }

    void ConsumePublished(FiberProfile& profile, CallCounters& counters)
    {
        ChangeReaderState(
            profile,
            FiberProfile::ReaderState::INACTIVE,
            FiberProfile::ReaderState::ACTIVE,
            std::memory_order_acquire
        );

        const auto writerIndex = profile.sharedWriterIndex.load(std::memory_order_acquire);
        if (writerIndex != FiberProfile::NO_PUBLISHED_BUFFER) {
            const auto readerIndex = writerIndex ^ 1U;
            ProcessBuffer(profile.buffers[readerIndex], 0, FiberProfile::CALL_BUFFER_SIZE, counters);
        }

        ChangeReaderState(
            profile,
            FiberProfile::ReaderState::ACTIVE,
            FiberProfile::ReaderState::INACTIVE,
            std::memory_order_release
        );
    }

    bool ConsumeProfile(FiberProfile& profile, CallCounters& counters)
    {
        const bool finished = !profile.active.load(std::memory_order_acquire);

        ConsumePublished(profile, counters);

        if (finished) {
            // Producer is permanently gone, so it is now safe to read
            // even the never-published, partially-filled active buffer.
            auto& current        = profile.buffers[profile.writerIndex];
            const uint32_t start = profile.count == FiberProfile::CALL_BUFFER_SIZE ? profile.cursor : 0;
            ProcessBuffer(current, start, profile.count, counters);
        }

        return finished;
    }

    void ConsumeActiveProfiles()
    {
        CallCounters counters;
        for (auto it = activeProfiles.begin(); it != activeProfiles.end();) {
            auto& profile = **it;
            if (ConsumeProfile(profile, counters)) {
                it = activeProfiles.erase(it);
            } else {
                ++it;
            }
        }
        ProcessCollectedCalls(counters);
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

    std::thread worker;
};

void FiberProfile::TryPublish()
{
    auto previous = readerState.load(std::memory_order_relaxed);
    while (previous == ReaderState::TRANSITION ||
           !readerState.compare_exchange_weak(
               previous,
               ReaderState::TRANSITION,
               std::memory_order_acq_rel,
               std::memory_order_relaxed
           )) {
        if (previous == ReaderState::TRANSITION) {
            std::this_thread::yield();
            previous = readerState.load(std::memory_order_relaxed);
        }
    }

    if (previous == ReaderState::INACTIVE) {
        writerIndex ^= 1U;
        cursor       = 0;
        count        = 0;

        sharedWriterIndex.store(writerIndex, std::memory_order_release);
    }

    readerState.store(previous, std::memory_order_release);
}

FiberProfileHandle CreateFiberProfile() { return FiberProfileManager::Instance().RegisterFiber(); }

void DestroyFiberProfile(FiberProfileHandle& profile)
{
    if (profile) {
        profile->active.store(false, std::memory_order_release);
        profile.reset();
    }
}

} // namespace Cbc::Profiler
