#pragma once

#include "engine/engine.h"
#include "interpreter/function_handle.h"
#include "runtimesupport/runtime.h"
#include "utils/logger.h"
#include <cstdint>
#include <optional>
#include <string_view>

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

extern Logging::Logger log;

class Resolver;

/// The handle that represents a type.
class Type {
public:
    ~Type() = default;

    /// Full name of the type.
    virtual std::string GetName() = 0;

    /// Runtime type info.
    virtual std::optional<RTSupport::TypeInfo> GetTypeInfo() = 0;
};

struct MethodSignature {
    std::vector<Type*> params;
    Type* resType;
};

struct DirectCall {
    Type* refType;
    std::string_view name;
    MethodSignature signature;
    Interpretation::TaggedFunctionHandle fuh;

    std::string GetFullName(Resolver& resolver);
};

struct DynamicCall {
    Type* refType;
    std::string_view name;
    MethodSignature signature;
    int methodNum;
    int extDefNum;

    std::string GetFullName(Resolver& resolver);
};

struct InstanceField {
    Type* refType;
    std::string_view name;
    Type* fieldType;
    uint32_t ordinal;
    std::optional<int> offset;

    std::string GetFullName(Resolver& resolver);
};

struct StaticField {
    Type* refType;
    std::string_view name;
    Type* fieldType;
    uintptr_t location;

    std::string GetFullName(Resolver& resolver);
};

template <typename T>
class Index {
public:
    explicit Index(uint16_t value) : value(value) {}
    int GetValue() const { return value; }
    bool operator==(Index const& index) const
    {
        return value == index.value;
    }
private:
    uint16_t value;
};

/// Resolver of identifiers in the context of `method`.
class Resolver {
public:
    Resolver(Engine::Session& session, Engine::Identifier<Symlevel::MethodDefinition> method);
    Resolver(Resolver&& another);
    ~Resolver();

    std::optional<Type*> Query(Index<Type> id);
    std::optional<DirectCall const*> Query(Index<DirectCall> id);
    std::optional<DynamicCall const*> Query(Index<DynamicCall> id);
    std::optional<InstanceField const*> Query(Index<InstanceField> id);
    std::optional<StaticField const*> Query(Index<StaticField> id);

    class Impl;
private:
    std::unique_ptr<Impl> impl;
};

}
