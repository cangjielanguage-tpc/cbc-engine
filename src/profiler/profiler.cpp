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
constexpr auto COLLECTION_PERIOD  = std::chrono::milliseconds(100);
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

        {
            std::lock_guard<std::mutex> lock(profilesMutex);
            activeProfiles.emplace_back(profile);
        }
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
            std::lock_guard<std::mutex> lock(profilesMutex);
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
        auto* bytecode  = callerFuh->bytecode.load();
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
        const auto callee                = bytecode->code.literals->at(args.xi12.imm12).uintptr;
        auto* calleeFuh                  = reinterpret_cast<Interpretation::DynamicFunctionHandle*>(callee);
        const auto runtimeCallSiteOffset = static_cast<uint32_t>(edge.callSite - bytecodeStart);
        const auto cbcCallSiteOffset =
            bytecode->offsetsIndex.FindMappedOffset(Cbc::InstructionType::REWRITTEN, runtimeCallSiteOffset);
        ASSERT(cbcCallSiteOffset.has_value());

        return {
            edge.caller,
            ResolveFunctionName(session, *callerFuh),
            cbcCallSiteOffset.value(),
            runtimeCallSiteOffset,
            callee,
            ResolveFunctionName(session, *calleeFuh),
            count,
            count >= HOT_CALL_COUNT,
        };
    }

    void ProcessCollectedCalls(const CallCounters& counters, uint64_t recordedCallCount, uint64_t scanTimeUs)
    {
        Engine::Session session(Engine::GetEngineInstance());
        std::vector<ProfiledCall> calls;
        calls.reserve(counters.size());
        for (const auto& [edge, count] : counters) {
            calls.emplace_back(ResolveCall(session, edge, count));
        }
        LogProfilerPass(calls, recordedCallCount, scanTimeUs);
    }

    void ProcessBuffer(const FiberProfile::CallBuffer& buffer, uint32_t start, uint32_t count, CallCounters& counters)
    {
        for (uint32_t i = 0; i < count; ++i) {
            const auto index   = (start + i) & (FiberProfile::CALL_BUFFER_SIZE - 1);
            const auto& record = buffer.records[index];
            ++counters[CallEdge { record.caller, record.callSite }];
        }
    }

    void ConsumePublished(FiberProfile& profile, CallCounters& counters)
    {
        std::lock_guard<std::mutex> lock(profile.bufferMutex);

        const auto writerIndex = profile.sharedWriterIndex;
        if (writerIndex != FiberProfile::NO_PUBLISHED_BUFFER) {
            const auto readerIndex = writerIndex ^ 1U;
            const auto& buffer     = profile.buffers[readerIndex];
            if (buffer.recordCount != profile.processedCount) {
                ProcessBuffer(buffer, 0, FiberProfile::CALL_BUFFER_SIZE, counters);
                profile.processedCount = buffer.recordCount;
            }
        }
    }

    bool ConsumeProfile(FiberProfile& profile, CallCounters& counters, uint64_t& recordedCallCount)
    {
        const bool finished = !profile.active.load();

        const auto currentRecordCount =
            profile.recordCount.load(std::memory_order_relaxed) + (finished ? profile.cursor : 0);
        recordedCallCount     += currentRecordCount - profile.observedCount;
        profile.observedCount  = currentRecordCount;

        ConsumePublished(profile, counters);

        if (finished) {
            const auto writerIndex = profile.sharedWriterIndex & 1U;
            auto& current          = profile.buffers[writerIndex];
            const uint32_t start   = profile.count == FiberProfile::CALL_BUFFER_SIZE ? profile.cursor : 0;
            ProcessBuffer(current, start, profile.count, counters);
        }

        return finished;
    }

    void ConsumeActiveProfiles()
    {
        CallCounters counters;
        uint64_t recordedCallCount = 0;
        const auto scanStart       = std::chrono::steady_clock::now();

        std::unique_lock<std::mutex> lock(profilesMutex);
        for (auto it = activeProfiles.begin(); it != activeProfiles.end();) {
            auto profile = *it;

            lock.unlock();
            const bool finished = ConsumeProfile(*profile, counters, recordedCallCount);
            lock.lock();

            if (finished) {
                it = activeProfiles.erase(it);
            } else {
                ++it;
            }
        }
        lock.unlock();

        const auto scanTime =
            std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - scanStart);
        ProcessCollectedCalls(counters, recordedCallCount, scanTime.count());
    }

    void Run()
    {
        std::unique_lock<std::mutex> lock(profilesMutex);
        while (true) {
            wakeup.wait_for(lock, COLLECTION_PERIOD, [&] { return stopping; });

            if (stopping) {
                return;
            }

            lock.unlock();
            ConsumeActiveProfiles();
            lock.lock();
        }
    }

    std::mutex profilesMutex;
    std::condition_variable wakeup;
    std::list<FiberProfileHandle> activeProfiles; // guarded by profilesMutex
    bool stopping { false };                      // guarded by profilesMutex

    std::thread worker;
};

FiberProfileHandle CreateFiberProfile() { return FiberProfileManager::Instance().RegisterFiber(); }

void DestroyFiberProfile(FiberProfileHandle& profile)
{
    if (profile) {
        profile->active.store(false);
        profile.reset();
    }
}

} // namespace Cbc::Profiler
