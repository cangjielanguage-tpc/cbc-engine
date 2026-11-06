#include "profiler.h"

#include "cbc/isa_rt.h"
#include "engine/decode/reader.h"
#include "inlining/inline_eligibility.h"
#include "inlining/inline_plan.h"
#include "interpreter/function_handle.h"
#include "profiler/profiler_logger.h"
#include "utils/assertion.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <list>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace Cbc::Profiler {

bool enabled = true;

namespace {
constexpr auto COLLECTION_PERIOD  = std::chrono::milliseconds(10);
constexpr uint64_t HOT_CALL_COUNT = 100;
} // namespace

struct InlineRequest {
    Interpretation::DynamicFunctionHandle* callerFuh;
    Interpretation::ExecBytecodeInfo* expectedBytecode;
    Interpretation::DynamicFunctionHandle* calleeFuh;
    uint32_t cbcCallSiteOffset;
    uint32_t runtimeCallSiteOffset;
    uint64_t count;
    uint64_t expectedVersion;
};

struct InlinePlanKey {
    Interpretation::DynamicFunctionHandle* callerFuh;
    Interpretation::ExecBytecodeInfo* expectedBytecode;
    uint64_t expectedVersion;

    bool operator==(const InlinePlanKey& other) const
    {
        return callerFuh == other.callerFuh && expectedBytecode == other.expectedBytecode &&
               expectedVersion == other.expectedVersion;
    }
};

struct InlinePlanKeyHash {
    size_t operator()(const InlinePlanKey& key) const
    {
        const auto callerHash   = std::hash<Interpretation::DynamicFunctionHandle*> {}(key.callerFuh);
        const auto bytecodeHash = std::hash<Interpretation::ExecBytecodeInfo*> {}(key.expectedBytecode);
        const auto versionHash  = std::hash<uint64_t> {}(key.expectedVersion);
        return callerHash ^ (bytecodeHash << 1) ^ (versionHash << 2);
    }
};

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

    static ProfiledCall ParseProfiledCall(Engine::Session& session, const InlineRequest& request)
    {
        return {
            reinterpret_cast<FunctionId>(request.callerFuh),
            ResolveFunctionName(session, *request.callerFuh),
            reinterpret_cast<uintptr_t>(request.expectedBytecode),
            reinterpret_cast<uintptr_t>(request.expectedBytecode->code.bytecode),
            static_cast<uint32_t>(request.expectedBytecode->code.bytecodeSize),
            request.cbcCallSiteOffset,
            request.runtimeCallSiteOffset,
            reinterpret_cast<FunctionId>(request.calleeFuh),
            ResolveFunctionName(session, *request.calleeFuh),
            request.count,
            request.count >= HOT_CALL_COUNT,
            request.expectedVersion,
        };
    }

    std::optional<InlineRequest> ResolveCall(const CallEdge& edge, uint64_t count) const
    {
        auto* callerFuh = reinterpret_cast<Interpretation::DynamicFunctionHandle*>(edge.caller);
        auto* bytecode  = callerFuh->bytecode.load();
        ASSERT(bytecode != nullptr);

        const auto bytecodeStart = reinterpret_cast<uintptr_t>(bytecode->code.bytecode);
        const auto bytecodeEnd   = bytecodeStart + bytecode->code.bytecodeSize;

        if (edge.callSite < bytecodeStart || edge.callSite >= bytecodeEnd ||
            bytecodeEnd - edge.callSite < Cbc::RT::B3xi12::SIZE) {
            return std::nullopt;
        }

        auto* callSite = reinterpret_cast<uint8_t*>(edge.callSite);
        Decoder::ByteReader reader(callSite, bytecode->code.bytecode, reinterpret_cast<uint8_t*>(bytecodeEnd));
        const auto args = Cbc::RT::B3xi12::Decode(reader);
        if (args.opc != Cbc::RT::Opcode::DIRECT_CALL_2I) {
            return std::nullopt;
        }

        const auto callee                = bytecode->code.literals->at(args.xi12.imm12).uintptr;
        auto* calleeFuh                  = reinterpret_cast<Interpretation::DynamicFunctionHandle*>(callee);
        const auto runtimeCallSiteOffset = static_cast<uint32_t>(edge.callSite - bytecodeStart);
        const auto cbcCallSiteOffset =
            bytecode->offsetsIndex.FindMappedOffset(Cbc::InstructionType::REWRITTEN, runtimeCallSiteOffset);
        if (!cbcCallSiteOffset.has_value()) {
            return std::nullopt;
        }

        return InlineRequest {
            callerFuh, bytecode, calleeFuh, *cbcCallSiteOffset, runtimeCallSiteOffset, count, bytecode->version,
        };
    }

    static std::vector<InlinePlan> BuildInlinePlans(
        Engine::Session& session, const std::vector<InlineRequest>& requests
    )
    {
        std::vector<InlinePlan> plans;
        std::unordered_map<InlinePlanKey, size_t, InlinePlanKeyHash> planIndices;

        for (const auto& request : requests) {
            auto* expectedCalleeBytecode = request.calleeFuh->bytecode.load();
            const auto eligibility =
                CheckInlineEligibility(session, request.callerFuh, request.calleeFuh, expectedCalleeBytecode);
            if (!eligibility.IsAllowed()) {
                Log::profiler.Log(Logging::Level::DEBUG, [&](Stream::Output& out) {
                    out.PrintLn(
                        "Inline rejected: caller {} ({}), callee {} ({}), cbc offset {}: {}",
                        static_cast<const void*>(request.callerFuh),
                        ResolveFunctionName(session, *request.callerFuh),
                        static_cast<const void*>(request.calleeFuh),
                        ResolveFunctionName(session, *request.calleeFuh),
                        request.cbcCallSiteOffset,
                        eligibility.reason
                    );
                });
                continue;
            }

            const InlinePlanKey key { request.callerFuh, request.expectedBytecode, request.expectedVersion };
            const auto [it, inserted] = planIndices.emplace(key, plans.size());
            if (inserted) {
                plans.push_back(InlinePlan {
                    request.callerFuh,
                    request.expectedBytecode,
                    request.expectedVersion,
                    {},
                });
            }
            plans[it->second].callSites.push_back(InlineCallSite {
                request.calleeFuh,
                expectedCalleeBytecode,
                expectedCalleeBytecode->version,
                request.cbcCallSiteOffset,
                request.runtimeCallSiteOffset,
            });
        }

        for (auto& plan : plans) {
            std::sort(plan.callSites.begin(), plan.callSites.end(), [](const auto& lhs, const auto& rhs) {
                return lhs.cbcCallSiteOffset < rhs.cbcCallSiteOffset;
            });
        }
        return plans;
    }

    void ProcessCollectedCalls(const CallCounters& counters, uint64_t recordedCallCount, uint64_t scanTimeUs)
    {
        Engine::Session session(Engine::GetEngineInstance());
        std::vector<ProfiledCall> callsFoLog;
        std::vector<InlineRequest> callsForInline;
        for (const auto& [edge, count] : counters) {
            auto call = ResolveCall(edge, count);
            if (!call.has_value()) {
                continue;
            }

            callsFoLog.emplace_back(ParseProfiledCall(session, *call));
            if (count >= HOT_CALL_COUNT) {
                callsForInline.emplace_back(*call);
            }
        }
        LogProfilerPass(callsFoLog, recordedCallCount, scanTimeUs);
        InlineCalls(session, BuildInlinePlans(session, callsForInline));
    }

    void InlineCalls(Engine::Session& session, const std::vector<InlinePlan>& plans)
    {
        auto& manager = Interpretation::FunctionHandleManager::Of(session);
        for (const auto& plan : plans) {
            Log::profiler.Log(Logging::Level::DEBUG, [&](Stream::Output& out) {
                out << "Rebuilding " << plan << Stream::endl;
            });

            auto* published =
                manager.RebuildAndPublish(session, plan.callerFuh, plan.expectedBytecode, plan.expectedVersion);
            Log::profiler.Log(Logging::Level::DEBUG, [&](Stream::Output& out) {
                if (published != nullptr) {
                    out.PrintLn(
                        "Rebuild succeeded: bytecode {} version {}, code {}+{}",
                        reinterpret_cast<const void*>(published),
                        published->version,
                        reinterpret_cast<const void*>(published->code.bytecode),
                        published->code.bytecodeSize
                    );
                } else {
                    out.PrintLn(
                        "Rebuild failed: bytecode {} version {} is no longer current",
                        reinterpret_cast<const void*>(plan.expectedBytecode),
                        plan.expectedVersion
                    );
                }
            });
        }
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
