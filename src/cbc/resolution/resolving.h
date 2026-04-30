#pragma once

#include "engine/engine.h"
#include "interpreter/function_handle.h"
#include "runtimesupport/runtime.h"
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

/// This namespace provides an facade to access symlevel from rewriter.
///
/// It encapsulates symlevel implementation details and memory management from users.
/// This layer mostly consists of the resolver itself, which is provides
/// an mapping from bytecode encoded indicies to the handles, through which symlevel can be accessed.
///
/// Handles are represented by instances of `Type`, `Field` or `Method`,
/// which are essentially an wrapper around references to corresponding entitites.
///
/// The handles are mostly consists of `optional` properties.
/// The presence of the property is decided by:
/// - actual kind of referenced entity;
/// - resolution success.
///
/// To work properly with handles in rewriter an specialized wrapper
/// is needed.

namespace Resolution {

/// The handle that represents a type.
class Type {
public:
    /// Full name of the type.
    std::string_view GetName();

    /// Runtime type info.
    std::optional<RTSupport::TypeInfo> GetTypeInfo();

    /// Number of fields in the given type.
    std::optional<int> FieldsNum();

    /// The size of a field of the given type.
    std::optional<int> FieldsSize();

private:
    class Impl;
    friend class Impl;
    Impl* impl;
};

struct MethodSignature {
    std::vector<Type> const& GetParamTypes();
    Type GetResultType();

private:
    class Impl;
    friend class Impl;
    Impl* impl;
};

/// The handle that represents a method reference (virtual, interface and static).
class Method {
public:
    /// Full name of the method (including signature).
    std::string_view GetName();

    /// The abi signature of the method.
    std::optional<MethodSignature> GetSignature();

    std::optional<Type> GetRefType();

    /// FunctionHandle of the method, that is needed for execution.
    std::optional<Interpretation::TaggedFunctionHandle> GetFuH();

    /// The number of the method in the table.
    std::optional<uint16_t> GetMethodNum();

    /// The number table in ref type.
    std::optional<uint16_t> ExtDefNum();

private:
    class Impl;
    friend class Impl;
    Impl* impl;
};

/// The handle that represents a field (instance and static).
class Field {
public:
    /// Full name of the method (including signature).
    std::string_view GetName();

    /// Type of the field.
    std::optional<Type> GetFieldType();

    std::optional<Type> GetRefType();

    /// The index of the field in total field numbering of ref type.
    int GetOrdinal();

    /// The offset of the field.
    std::optional<uint32_t> GetOffset();

    /// The location of the field.
    std::optional<std::uintptr_t> GetLocation();

private:
    class Impl;
    friend class Impl;
    Impl* impl;
};

template <typename T>
class Index {
    Index(uint16_t value);
    uint16_t GetValue();
private:
    uint16_t value;
};

/// Resolver of identifiers in the context of `method`.
class Resolver {
public:
    Resolver(Engine::Session& session, Engine::Identifier<Symlevel::MethodDefinition> method);

    Type Resolve(Index<Type> id);
    Method ResolveStatic(Index<Method> id);
    Method ResolveDynamic(Index<Method> id);
    Field ResolveInstance(Index<Field> id);
    Field ResolveStatic(Index<Field> id);
};

}
