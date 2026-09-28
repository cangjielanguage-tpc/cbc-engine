#include <atomic>
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
#include "engine/image/reader.h"
#include "engine/resolving_output.h"
#include "function_handle.h"
#include "interpreter/code.h"
#include "interpreter/loggers.h"
#include "resolution/resolution.h"
#include "runtimesupport/adapters.h"
#include "utils/assertion.h"
#include "utils/misc.h"
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
    std::unordered_map<std::string, DynamicFunctionHandle*> helpers;
};

FunctionHandleManager::FunctionHandleManager() : impl(std::make_unique<FunctionHandleManager::Impl>()) {}

FunctionHandleManager::~FunctionHandleManager()                               = default;
FunctionHandleManager::FunctionHandleManager(FunctionHandleManager&& manager) = default;

[[noreturn]] static void UnresolvedAotMethodCalled() { FATAL("Called unresolved AOT method"); }

[[noreturn]] static void HelperWasNotFound(std::string_view methodName)
{
    FATAL("Helper method %s was not found", methodName);
}

static DynamicFunctionHandle* FindAndPrepareHelper(
    Session& session, FunctionHandleManager& fhm, std::string_view methodName
)
{
    // FIXME: change this to intrinsics.cbc in future iterations
    auto fileName = "default.cbc";
    // FIXME: change name of type
    //
    auto typeName  = "$P$VERY_COOL_PACKAGE";
    auto methodOpt = session.GetEngine().FindMethod(session, fileName, typeName, methodName);
    UNWRAP_OPT_NORETURN(method, session.GetEngine().FindMethod(session, fileName, typeName, methodName), [&]() {
        HelperWasNotFound(methodName);
    });

    auto taggedHandle = fhm.AcquireTagged(session, method);
    if (auto* dynFuh = std::get_if<DynamicFunctionHandle*>(&taggedHandle)) {
        auto fuh = *dynFuh;
        auto res = fhm.Prepare(session, fuh);
        if (!res.success) {
            FATAL("Failure during preparation of helper method %s", methodName);
        }
        return fuh;
    } else {
        FATAL("Helper method %s was found in AOT code instead of cbc", methodName);
    }
}

void FunctionHandleManager::FindAndPrepareHelpers(Session& session)
{
    auto helperName = "foo_helper";
    auto fuh        = FindAndPrepareHelper(session, *this, helperName);
    this->impl->helpers.insert({ helperName, fuh });
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
        auto mem = new (std::nothrow) StaticFunctionHandle(fuh);
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
        auto mem    = new (std::nothrow) DynamicFunctionHandle(i2Call, c2Call, methodDef);
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
        auto dynFuh   = impl->helpers["foo_helper"];
        auto byteCode = dynFuh->bytecode.load();
        return PreparationResult::Fail(byteCode, dynFuh);
    }

    auto bc = new (std::nothrow) ExecBytecodeInfo(bytecode.value());
    if (bc == nullptr)
        FATAL("Out of memory");

    // ensure `bc` content writes completes before publication.
    std::atomic_thread_fence(std::memory_order_seq_cst);
    fuh->bytecode.store(bc, std::memory_order_relaxed);
    return PreparationResult::Success(bc, fuh);
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
