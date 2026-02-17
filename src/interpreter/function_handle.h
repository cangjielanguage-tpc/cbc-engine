#pragma once

#include <cstddef>
#include <atomic>
#include <mutex>

#include "code.h"
#include "ectype.h"
#include "runtime.h"
#include "engine/engine.h"

namespace Interpretation {

class FunctionHandle;
class DynamicFunctionHandle;

/// An opaque handle to symlevel method definition.
using MethodDefIdentifier = void*;

using ABIDesc = void*;

/// The function that would be called to perform a FuH invocation from interpreted code.
/// The ABI is passed from the callsite.
using I2Call = void (*)(FunctionHandle*, ABIDesc, Ectype*, ThreadHandle);

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
    FunctionHandle(I2Call i2call)
        : i2call(i2call) {};

    I2Call i2call;
};

/// Function handle of cbc-provided function.
struct DynamicFunctionHandle : public FunctionHandle {
    DynamicFunctionHandle(I2Call i2Call, C2Call c2call, ABIDesc desc, MethodDefIdentifier methodDef) :
        FunctionHandle(i2Call),
        c2call(c2call),
        desc(desc),
        descriptor(nullptr),
        lock(),
        methodDef(methodDef)
    {}

    C2Call c2call;

    /// Eagerly initialized.
    /// Holds the information about an ABI conversions to make a C2I invocation.
    ABIDesc const desc;

    /// Lazily initialized.
    /// Holds the information about a frame of interpreted method and bytecode itself.
    std::atomic<FuHDescriptor*> descriptor;
    std::mutex lock;

    MethodDefIdentifier const methodDef;
};


/// Represents AOT compiled function.
///
/// Note that not all AOT functions may be called using `StaticFunctionHandle`.
/// This kind of `FunctionHandle` is mainly needed to work with methods of
/// dynamically created `TypeInfo` in the similar way, both for cbc and aot functions.
struct StaticFunctionHandle : public FunctionHandle {
    void* const function;
};

/// Acquires an FunctionHandle for given method definition.
FunctionHandle* AcquireFunctionHandle(MethodDefIdentifier methodDef);

/// Performs lazy initialization of a DynamicFunctionHandle.
FuHDescriptor* PrepareDynamicFuH(Engine::Session& session, DynamicFunctionHandle* fuh);

} // namespace Interpretation
