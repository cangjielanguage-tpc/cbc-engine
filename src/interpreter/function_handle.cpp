#include <atomic>
#include <limits>
#include <mutex>
#include <new>
#include <unordered_map>
#include <variant>

#include "adapters.h"
#include "cbc/isa_rewriter.h"
#include "engine/image/flags.h"
#include "engine/image/reader.h"
#include "engine/resolving_output.h"
#include "function_handle.h"
#include "interpreter/loggers.h"
#include "resolution/resolution.h"
#include "runtimesupport/adapters.h"
#include "utils/assertion.h"
#include "utils/ostream.h"

namespace Interpretation {

static_assert(offsetof(DynamicFunctionHandle, c2call) == FUNCTION_HANDLE_C2CALL_OFFSET);
static_assert(offsetof(DynamicFunctionHandle, bytecode) == FUNCTION_HANDLE_BYTECODE_OFFSET);

using namespace Engine;
using namespace Image;

class FunctionHandleManager::Impl {
public:
    using Ident = Image::Identifier<MethodDefinition>;
    std::mutex lock;
    std::unordered_map<Ident::Packed, TaggedFunctionHandle> fuhMap;
};

FunctionHandleManager::FunctionHandleManager() : impl(std::move(std::make_unique<FunctionHandleManager::Impl>())) {}

FunctionHandleManager::~FunctionHandleManager()                               = default;
FunctionHandleManager::FunctionHandleManager(FunctionHandleManager&& manager) = default;

[[noreturn]] static void UnresolvedAotMethodCalled()
{
    FATAL("Called unresolved AOT method");
}

TaggedFunctionHandle FunctionHandleManager::AcquireTagged(
    Session& session, Image::Identifier<Image::MethodDefinition> methodDef
)
{
    std::lock_guard guard(impl->lock);
    auto res = impl->fuhMap.find(methodDef.Pack());
    if (res != impl->fuhMap.end()) {
        return res->second;
    }

    auto method = Decode::Read(session, methodDef);

    LOGS_INFO(Log::preparation, session, "started to build fuh for {}", method);

    auto flags = method.GetFlags();
    ASSERTION(!flags.Is(MethodFlag::ABSTRACT), "Only methods that can be actually called can have FUH");

    auto newStaticFuh = [&]() -> StaticFunctionHandle* {
        auto& deps       = session.GetEngine().Dependencies().At(methodDef.GetFileId());
        auto linkageName = Decode::Read(session, method.LinkageName().value());
        auto target      = deps.FindSymbol(linkageName);

        if (target == nullptr) {
            LOGS_ERROR(
                Log::preparation, session, "failed to resolve aot method {}\n  linkage name: {}", method, linkageName
            );
            // TODO: put stub trampoline that throws exception
            target = reinterpret_cast<void*>(&UnresolvedAotMethodCalled);
        }

        StaticFunctionHandle fuh {
            .base     = FunctionHandle(RTSupport::Adapters::GenericI2CCallInstance()),
            .function = target,
        };
        auto mem = new(std::nothrow) StaticFunctionHandle(fuh);
        if (mem == nullptr) {
            FATAL("out of memory");
        }
        // public content of `mem`.
        std::atomic_thread_fence(std::memory_order_seq_cst);
        return mem;
    };

    auto newDynFuh = [&]() -> DynamicFunctionHandle* {
        auto i2Call = PrepareI2Call(session, methodDef);
        auto c2Call = PrepareC2Call(session, methodDef);
        auto mem    = new(std::nothrow) DynamicFunctionHandle(i2Call, c2Call, methodDef);
        if (mem == nullptr) {
            FATAL("out of memory");
        }
        // public content of `mem`.
        std::atomic_thread_fence(std::memory_order_seq_cst);
        return mem;
    };

    auto fuh = flags.Is(MethodFlag::AOT) ? TaggedFunctionHandle(newStaticFuh()) : TaggedFunctionHandle(newDynFuh());

    impl->fuhMap.insert({ methodDef.Pack(), fuh });
    return fuh;
}

FunctionHandle* FunctionHandleManager::Acquire(Session& session, Image::Identifier<Image::MethodDefinition> methodDef)
{
    auto fuh = AcquireTagged(session, methodDef);
    if (std::holds_alternative<DynamicFunctionHandle*>(fuh)) {
        return &std::get<DynamicFunctionHandle*>(fuh)->base;
    } else {
        return &std::get<StaticFunctionHandle*>(fuh)->base;
    }
}

ExecBytecodeInfo* FunctionHandleManager::Prepare(Session& session, DynamicFunctionHandle* fuh)
{
    std::lock_guard guard(fuh->lock);

    if (auto bytecode = fuh->bytecode.load(); bytecode != nullptr) {
        return bytecode;
    }

    auto& logger = Interpretation::Log::preparation;

    LOGS_INFO(
        logger, session, "started preparation of method (fuh={}) {}", Stream::Hex(fuh), Stream::Detailed(fuh->methodDef)
    );

    Resolution::Resolver resolver(session, fuh->methodDef);

    auto& heap    = session.GetEngine().CodeHeap();
    auto bytecode = Cbc::Rewrite(session, fuh->methodDef, heap);

    auto ownedBytecode = std::unique_ptr<ExecBytecodeInfo>(new(std::nothrow) ExecBytecodeInfo(bytecode));
    if (ownedBytecode == nullptr) FATAL("Out of memory");
    ownedBytecode->ownerFuh = fuh;
    auto* bc = ownedBytecode.get();

    fuh->bytecodeVersions.emplace_back(std::move(ownedBytecode));
    fuh->bytecode.store(bc);
    return bc;
}

namespace {

bool CodeRangesOverlap(const ExecBytecodeInfo& lhs, const ExecBytecodeInfo& rhs)
{
    const auto lhsStart = reinterpret_cast<uintptr_t>(lhs.code.bytecode);
    const auto rhsStart = reinterpret_cast<uintptr_t>(rhs.code.bytecode);
    ASSERT(lhs.code.bytecodeSize <= std::numeric_limits<uintptr_t>::max() - lhsStart);
    ASSERT(rhs.code.bytecodeSize <= std::numeric_limits<uintptr_t>::max() - rhsStart);
    const auto lhsEnd = lhsStart + lhs.code.bytecodeSize;
    const auto rhsEnd = rhsStart + rhs.code.bytecodeSize;
    return lhsStart < rhsEnd && rhsStart < lhsEnd;
}

} // namespace

ExecBytecodeInfo* FunctionHandleManager::RebuildAndPublish(
    Session& session,
    DynamicFunctionHandle* fuh,
    ExecBytecodeInfo* expectedBytecode,
    uint64_t expectedVersion
)
{
    auto* snapshot = fuh->bytecode.load();
    if (snapshot != expectedBytecode || snapshot->version != expectedVersion) {
        return nullptr;
    }
    ASSERT(expectedVersion != std::numeric_limits<uint64_t>::max());

    auto& heap = session.GetEngine().CodeHeap();
    auto rebuilt = std::unique_ptr<ExecBytecodeInfo>(
        new(std::nothrow) ExecBytecodeInfo(Cbc::Rewrite(session, fuh->methodDef, heap))
    );
    if (rebuilt == nullptr) FATAL("Out of memory");
    rebuilt->version = expectedVersion + 1;
    rebuilt->ownerFuh = fuh;

    std::lock_guard guard(fuh->lock);
    auto* current = fuh->bytecode.load();
    if (current != expectedBytecode || current->version != expectedVersion) {
        return nullptr;
    }

    for (const auto& version : fuh->bytecodeVersions) {
        ASSERT(!CodeRangesOverlap(*version, *rebuilt));
    }

    auto* published = rebuilt.get();
    fuh->bytecodeVersions.emplace_back(std::move(rebuilt));
    fuh->bytecode.store(published);
    return published;
}

void* FunctionHandleManager::GetFunctionPtrForDirectCall(TaggedFunctionHandle fuh)
{
    if (auto* staticFuh = std::get_if<StaticFunctionHandle*>(&fuh)) {
        return (*staticFuh)->function;
    } else {
        auto dynFuh = std::get<DynamicFunctionHandle*>(fuh);
        return GetDirectCallTrampoline(dynFuh);
    }
}

} // namespace Interpretation
