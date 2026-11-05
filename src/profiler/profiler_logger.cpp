#include "profiler_logger.h"

#include "profiler/profiler.h"
#include "utils/ostream.h"

#include <algorithm>
#include <numeric>

namespace Cbc::Profiler {

Stream::Descripted Log::stream(Stream::cerr, "[profiler] ");
Logging::Logger Log::profiler(&Log::stream);

namespace {
uint64_t processingNumber = 0;
} // namespace

void LogProfilerPass(const std::vector<ProfiledCall>& calls, uint64_t recordedCallCount, uint64_t scanTimeUs)
{
    processingNumber++;

    Log::profiler.Log(Logging::Level::DEBUG, [&](Stream::Output& out) {
        const auto totalCalls = std::accumulate(
            calls.begin(), calls.end(), uint64_t { 0 },
            [](uint64_t total, const ProfiledCall& call) { return total + call.count; }
        );

        out.PrintLn(
            "Processing #{}: {} calls recorded, {} calls collected at {} call sites, buffer scan took {} us",
            processingNumber,
            recordedCallCount,
            totalCalls,
            calls.size(),
            scanTimeUs
        );

        std::vector<const ProfiledCall*> sortedCalls;
        sortedCalls.reserve(calls.size());
        for (const auto& call : calls) {
            sortedCalls.emplace_back(&call);
        }
        std::sort(sortedCalls.begin(), sortedCalls.end(), [](const ProfiledCall* lhs, const ProfiledCall* rhs) {
            return lhs->count > rhs->count;
        });

        for (const auto* call : sortedCalls) {
            out.PrintLn(
                "  [{}] {} version: {} bytecode: {} code: {}+{} ({}) +cbc:{} +rt:{} -> {} ({}): {} calls",
                call->hot ? "HOT" : "cold",
                reinterpret_cast<const void*>(call->caller),
                call->version,
                reinterpret_cast<const void*>(call->bytecodeInfo),
                reinterpret_cast<const void*>(call->codeStart),
                call->codeSize,
                call->callerName,
                call->cbcCallSiteOffset,
                call->runtimeCallSiteOffset,
                reinterpret_cast<const void*>(call->callee),
                call->calleeName,
                call->count
            );
        }
    });
}

} // namespace Cbc::Profiler
