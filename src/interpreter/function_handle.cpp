#include <atomic>
#include <mutex>
#include <new>
#include <unordered_map>
#include <variant>

#include "adapters.h"
#include "cbc/isa_rewriter.h"
#include "engine/resolving_output.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/dependencies.h"
#include "engine/symlevel/flags.h"
#include "engine/symlevel/reader.h"
#include "function_handle.h"
#include "interpreter/loggers.h"
#include "resolution/resolution.h"
#include "runtimesupport/adapters.h"
#include "utils/assertion.h"

namespace Interpretation {

static_assert(offsetof(DynamicFunctionHandle, c2call) == FUNCTION_HANDLE_C2CALL_OFFSET);
static_assert(offsetof(DynamicFunctionHandle, bytecode) == FUNCTION_HANDLE_BYTECODE_OFFSET);

using namespace Engine;
using namespace Symlevel;

class FunctionHandleManager::Impl {
public:
    using Ident = Identifier<MethodDefinition>;
    std::mutex lock;
    std::unordered_map<Ident::Packed, TaggedFunctionHandle, Ident::Hasher> fuhMap;
};

FunctionHandleManager::FunctionHandleManager() : impl(std::move(std::make_unique<FunctionHandleManager::Impl>())) {}

FunctionHandleManager::~FunctionHandleManager()                               = default;
FunctionHandleManager::FunctionHandleManager(FunctionHandleManager&& manager) = default;

TaggedFunctionHandle FunctionHandleManager::AcquireTagged(
    Session& session, Identifier<Symlevel::MethodDefinition> methodDef
)
{
    std::lock_guard guard(impl->lock);
    auto res = impl->fuhMap.find(methodDef.Pack());
    if (res != impl->fuhMap.end()) {
        return res->second;
    }

    auto method = Symlevel::Reader::Read(session, methodDef);

    Log::preparation.Log(Logging::Level::TRACE, [&](Stream::Output& out) {
        Stream::ResolvingOutput stream(session, out);
        stream << "starting to build fuh for " << methodDef << " (" << Stream::Detailed(method.TypeName()) << "."
               << Stream::Detailed(method.Name()) << Stream::Detailed(method.Signature()) << ")" << Stream::endl;
    });

    auto flags = method.GetFlags();
    ASSERTION(!flags.Is(MethodFlag::ABSTRACT), "Only methods that can be actually called can have FUH");

    auto newStaticFuh = [&]() -> StaticFunctionHandle* {
        auto& deps       = session.CbcFileOf(methodDef.GetFileId()).GetDependencies();
        auto linkageName = Symlevel::Reader::Read(session, method.LinkageName().value());
        auto target      = deps.FindTarget(linkageName);

        Log::preparation.Log(Logging::Level::ERROR, [&](Stream::Output& out) {
            if (target != nullptr)
                return;
            using namespace Stream;
            Stream::ResolvingOutput stream(session, out);
            stream << "failed to resolve aot method" << endl;
            stream << "  name: " << Detailed(method.TypeName()) << "." << Detailed(method.Name())
                   << Detailed(method.Signature()) << endl;
            stream << "  linkageName: " << linkageName << endl;
        });

        // TODO: put stub trampoline that throws exception
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

FunctionHandle* FunctionHandleManager::Acquire(Session& session, Identifier<Symlevel::MethodDefinition> methodDef)
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

    logger.Log(Logging::Level::INFO, [&](Stream::Output& out) {
        using namespace Stream;
        auto def = Reader::Read(session, fuh->methodDef);
        Stream::ResolvingOutput stream(session, out);
        stream << endl << fuh->methodDef << " started preparation of method " << endl;
        stream << "  fuh: " << fuh << endl;
        stream << "  name: " << Detailed(def.TypeName()) << '.' << Detailed(def.Name()) << Detailed(def.Signature())
               << endl;
    });

    Resolution::Resolver resolver(session, fuh->methodDef);

    auto& heap    = session.GetEngine().CodeHeap();
    auto bytecode = Cbc::Rewrite(session, fuh->methodDef, heap);

    auto bc = new(std::nothrow) ExecBytecodeInfo(bytecode);
    if (bc == nullptr) FATAL("Out of memory");

    // ensure `bc` content writes completes before publication.
    std::atomic_thread_fence(std::memory_order_seq_cst);
    fuh->bytecode.store(bc, std::memory_order_relaxed);
    return bc;
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
