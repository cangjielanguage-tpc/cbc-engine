#pragma once

#include "engine/engine.h"
#include "interpreter/function_handle.h"
#include "runtimesupport/runtime.h"
#include "utils/logger.h"
#include "utils/ostream.h"
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

enum class CbcTypeKind {
    INVALID,
    VOID,
    BOOL,
    I8,
    U8,
    I16,
    U16,
    I32,
    U32,
    I64,
    U64,
    REF,
    REC,
    F32,
    F64,
};

/// The handle that represents a type.
class Type {
public:
    ~Type() = default;

    /// Full name of the type.
    virtual void GetFullName(Stream::Output& stream) const = 0;

    /// Runtime type info.
    virtual std::optional<RTSupport::TypeInfo> GetTypeInfo() = 0;

    virtual CbcTypeKind GetKind() = 0;
};

struct MethodSignature {
    std::vector<Type*> params;
    Type* resType;
};

struct DirectCall {
    struct Compiled {
        uintptr_t funcPtr;
        Interpretation::I2Call i2cAdapter;
    };

    using CallData = std::variant<Compiled, Interpretation::DynamicFunctionHandle*>;

    Type* refType;
    std::string_view name;
    MethodSignature signature;
    CallData data;
};

struct DynamicCall {
    Type* refType;
    std::string_view name;
    MethodSignature signature;
    int methodNum;
    int extDefNum;
};

struct InterfaceCall {
    Type* refType;
    std::string_view name;
    MethodSignature signature;
    int methodNum;
};

struct InstanceField {
    Type* refType;
    std::string_view name;
    Type* fieldType;
    uint32_t ordinal;
    std::optional<int> offset;
};

struct StaticField {
    Type* refType;
    std::string_view name;
    Type* fieldType;
    uintptr_t location;
};

Stream::Output& operator<<(Stream::Output& stream, Type const& type);
Stream::Output& operator<<(Stream::Output& stream, MethodSignature const& sig);
Stream::Output& operator<<(Stream::Output& stream, DirectCall const& call);
Stream::Output& operator<<(Stream::Output& stream, DynamicCall const& call);
Stream::Output& operator<<(Stream::Output& stream, InstanceField const& field);
Stream::Output& operator<<(Stream::Output& stream, StaticField const& field);

template <typename T> class Index {
public:
    explicit Index(uint16_t value) : value(value) {}

    int GetValue() const { return value; }

    bool operator==(Index const& index) const { return value == index.value; }

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
    std::optional<InterfaceCall const*> Query(Index<InterfaceCall> id);
    std::optional<InstanceField const*> Query(Index<InstanceField> id);
    std::optional<StaticField const*> Query(Index<StaticField> id);

    class Impl;

private:
    std::unique_ptr<Impl> impl;
};

} // namespace Resolution
