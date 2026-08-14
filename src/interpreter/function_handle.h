#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <variant>

#include "code.h"
#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/terms.h"

namespace API {
class Resolver;
} // namespace API

namespace Interpretation {

class FunctionHandle;
class DynamicFunctionHandle;
class StaticFunctionHandle;

using TaggedFunctionHandle = std::variant<DynamicFunctionHandle*, StaticFunctionHandle*>;

/// The function that would be called to perform a FuH invocation from interpreted code.
using I2Call = void*;

/// The function that would be called to perform a FuH invocation from compiled code.
/// The ABI of this function is custom and only used in hand-written assembly.
using C2Call = void*;

/// Represents an instance of a function that interpreter could work with.
/// Mainly, provides two ways to work with:
/// - calling via trampoline from compiled code (exclusive for `DynamicFunctionHandle`);
/// - calling from an interpreter;
///
/// For both cases, there is a specific function pointer `I2Call`/`C2Call` to do that.
/// These functions are encapsulating the behaviour and the data behind
/// an instance of `FunctionHandle` from the "function executor" perspective.
///
/// Each instance is created in runtime and associated with corresponding `MethodDefinition`
/// which contains all the necessary information about the method.
/// Preparation of function handle (especially dynamic version) is performed lazily.
struct FunctionHandle {
    FunctionHandle(I2Call i2call) : i2call(i2call) {};

    I2Call i2call;
};

/// Function handle of cbc-provided function.
struct DynamicFunctionHandle {
    DynamicFunctionHandle(I2Call i2Call, C2Call c2call, Symlevel::Identifier<Symlevel::MethodDefinition> methodDef)
        : base(i2Call),
          c2call(c2call),
          bytecode(nullptr),
          lock(),
          methodDef(methodDef)
    {}

    // Can not use inheritance because of standard layout rules.
    FunctionHandle base;

    C2Call c2call;

    /// Lazily initialized.
    /// Holds the information about a frame of interpreted method and bytecode itself.
    std::atomic<ExecBytecodeInfo*> bytecode;
    std::mutex lock;

    Symlevel::Identifier<Symlevel::MethodDefinition> const methodDef;
};

/// Represents AOT compiled function.
///
/// Note that not all AOT functions may be called using `StaticFunctionHandle`.
/// This kind of `FunctionHandle` is mainly needed to work with methods of
/// dynamically created `TypeInfo` in the similar way, both for cbc and aot functions.
struct StaticFunctionHandle {
    // Can not use inheritance because of standard layout rules.
    FunctionHandle base;
    void* const function;
};

class FunctionHandleManager {
public:
    static FunctionHandleManager& Of(Engine::Engine& engine);
    static FunctionHandleManager& Of(Engine::Session& session);
    FunctionHandleManager();
    FunctionHandleManager(FunctionHandleManager&& manager);
    ~FunctionHandleManager();

    /// Acquires an FunctionHandle for given method definition.
    FunctionHandle* Acquire(Engine::Session& session, Symlevel::Identifier<Symlevel::MethodDefinition> methodDef);
    TaggedFunctionHandle AcquireTagged(
        Engine::Session& session, Symlevel::Identifier<Symlevel::MethodDefinition> methodDef
    );

    /// Performs lazy initialization of a DynamicFunctionHandle.
    ExecBytecodeInfo* Prepare(Engine::Session& session, DynamicFunctionHandle* fuh);

    // Acquires a function pointer that could be invoked from compiled code
    // to invoke the method referenced by `fuh`.
    //
    // Note that result is neither `I2Call` nor `C2Call` and must be invoked from compiled code
    // with correspondig ABI.
    //
    // This function returns trampoline that is needed for direct call, usually for main method.
    void* GetFunctionPtrForDirectCall(TaggedFunctionHandle fuh);

private:
    class Impl;
    friend class Impl;

    std::unique_ptr<Impl> impl;
};

} // namespace Interpretation
