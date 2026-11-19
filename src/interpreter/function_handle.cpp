#include <array>
#include <atomic>
#include <limits>
#include <mutex>
#include <new>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

#include "adapters.h"
#include "cbc/isa_rewriter.h"
#include "engine/engine.h"
#include "engine/image/flags.h"
#include "engine/resolving_output.h"
#include "function_handle.h"
#include "inlining/inline_plan.h"
#include "inlining/runtime_inliner.h"
#include "interpreter/code.h"
#include "interpreter/loggers.h"
#include "resolution/resolution.h"
#include "runtimesupport/adapters.h"
#include "utils/assertion.h"
#include "utils/misc.h"
#include "utils/ostream.h"

extern "C" {
bool engine_store_debug_names;
}

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
    std::array<DynamicFunctionHandle*, static_cast<size_t>(Helper::Count)> helpers;
};

FunctionHandleManager::FunctionHandleManager() : impl(std::make_unique<FunctionHandleManager::Impl>()) {}

FunctionHandleManager::~FunctionHandleManager()                               = default;
FunctionHandleManager::FunctionHandleManager(FunctionHandleManager&& manager) = default;

[[noreturn]] static void UnresolvedAotMethodCalled() { FATAL("Called unresolved AOT method"); }

[[noreturn]] static void HelperWasNotFound(std::string_view methodName)
{
    FATAL("Helper method %.*s was not found", static_cast<int>(methodName.size()), methodName.data());
}

std::unordered_map<Helper, std::string> helperNames = {
    { Helper::ThrowAbstractMethodCallError, "throwAbstractMethodCallError" },
    { Helper::ThrowSymbolResolutionError, "throwSymbolResolutionError" },
};

void FunctionHandleManager::SetHelper(Helper helper, DynamicFunctionHandle* handle)
{
    impl->helpers[static_cast<size_t>(helper)] = handle;
}

DynamicFunctionHandle* FunctionHandleManager::GetHelper(Helper helper)
{
    auto handle = impl->helpers[static_cast<size_t>(helper)];
    ASSERT(handle != nullptr);
    return handle;
}

static DynamicFunctionHandle* FindAndPrepareHelper(
    Session& session, FunctionHandleManager& fhm, std::string_view methodName
)
{
    auto typeName = "$P$cbc-intrinsics";
    auto type     = session.GetEngine().FindType(session, typeName);
    UNWRAP_OPT_NORETURN(typeId, type, [&]() { HelperWasNotFound(methodName); });

    auto method = session.GetEngine().FindMethodInType(session, typeId, methodName);
    UNWRAP_OPT_NORETURN(methodId, method, [&]() { HelperWasNotFound(methodName); });

    auto taggedHandle = fhm.AcquireTagged(session, methodId);
    if (auto* dynFuh = std::get_if<DynamicFunctionHandle*>(&taggedHandle)) {
        auto fuh = *dynFuh;
        auto res = fhm.Prepare(session, fuh);
        if (!res.success) {
            FATAL(
                "Failure during preparation of helper method %.*s",
                static_cast<int>(methodName.size()),
                methodName.data()
            );
        }
        return fuh;
    } else {
        FATAL(
            "Helper method %.*s was found in AOT code instead of cbc",
            static_cast<int>(methodName.size()),
            methodName.data()
        );
    }
}

void FunctionHandleManager::FindAndPrepareHelpers(Session& session)
{
    for (const auto& [helper, methodName] : helperNames) {
        auto fuh = FindAndPrepareHelper(session, *this, methodName);
        SetHelper(helper, fuh);
    }
}

static char* MethodName(Session& session, Image::MethodDefinition methodDef)
{
    Stream::StringBuffer buf;
    Stream::ResolvingOutput out(session, buf);
    out << methodDef;
    return buf.ToCString();
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
        auto& fLibs      = session.GetEngine().ForeignLibs().At(methodDef.GetFileId());
        auto linkageName = Decode::Read(session, method.LinkageName().value());
        auto target      = flags.Is(MethodFlag::FOREIGN) ? fLibs.FindSymbol(linkageName) : deps.FindSymbol(linkageName);

        if (target == nullptr) {
            LOGS_ERROR(
                Log::preparation, session, "failed to resolve aot method {}\n  linkage name: {}", method, linkageName
            );
            // TODO: put stub trampoline that throws exception
            target = reinterpret_cast<void*>(&UnresolvedAotMethodCalled);
        }

        StaticFunctionHandle fuh {
            .function = target,
        };
        auto mem = new (std::nothrow) StaticFunctionHandle(fuh);
        if (mem == nullptr) {
            FATAL("out of memory");
        }
        // public content of `mem`.
        std::atomic_thread_fence(std::memory_order_seq_cst);
        return mem;
    };

    auto newDynFuh = [&]() -> DynamicFunctionHandle* {
        auto c2Call = PrepareC2Call(session, methodDef);

        char* name = nullptr;
        if (engine_store_debug_names) {
            name = MethodName(session, method);
        }

        auto mem = new (std::nothrow) DynamicFunctionHandle(c2Call, methodDef, name);
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

PreparationResult FunctionHandleManager::Prepare(Session& session, DynamicFunctionHandle* fuh)
{
    std::lock_guard guard(fuh->lock);

    if (auto bytecode = fuh->bytecode.load(); bytecode != nullptr) {
        return PreparationResult::Success(bytecode, fuh);
    }

    auto& logger = Interpretation::Log::preparation;

    LOGS_INFO(
        logger, session, "started preparation of method (fuh={}) {}", Stream::Hex(fuh), Stream::Detailed(fuh->methodDef)
    );

    Resolution::Resolver resolver(session, fuh->methodDef);

    auto& heap    = session.GetEngine().CodeHeap();
    auto bytecode = Cbc::Rewrite(session, fuh->methodDef, heap);

    if (!bytecode.has_value()) {
        auto dynFuh   = GetHelper(Helper::ThrowSymbolResolutionError);
        auto byteCode = dynFuh->bytecode.load();
        return PreparationResult::Fail(byteCode, dynFuh);
    }

    auto ownedBytecode = std::unique_ptr<ExecBytecodeInfo>(
        new(std::nothrow) ExecBytecodeInfo(std::move(bytecode.value()))
    );
    if (ownedBytecode == nullptr) FATAL("Out of memory");
    ownedBytecode->ownerFuh = fuh;
    auto* bc = ownedBytecode.get();

    fuh->bytecodeVersions.emplace_back(std::move(ownedBytecode));

    // ensure `bc` content writes completes before publication.
    std::atomic_thread_fence(std::memory_order_seq_cst);
    fuh->bytecode.store(bc, std::memory_order_relaxed);
    return PreparationResult::Success(bc, fuh);
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

ExecBytecodeInfo* FunctionHandleManager::RebuildAndPublish(Session& session, const Cbc::InlinePlan& plan)
{
    auto* fuh              = plan.callerFuh;
    auto* expectedBytecode = plan.expectedBytecode;
    auto expectedVersion   = plan.expectedVersion;
    auto* snapshot         = fuh->bytecode.load();
    if (snapshot != expectedBytecode || snapshot->version != expectedVersion) {
        return nullptr;
    }
    ASSERT(expectedVersion != std::numeric_limits<uint64_t>::max());

    auto& heap = session.GetEngine().CodeHeap();
    auto bytecode = Cbc::Rebuild(plan, heap);
    if (!bytecode.has_value()) {
        return nullptr;
    }
    auto rebuilt = std::unique_ptr<ExecBytecodeInfo>(
        new(std::nothrow) ExecBytecodeInfo(std::move(bytecode.value()))
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
