#pragma once

#include "engine/engine.h"
#include "engine/field_layout.h"
#include "engine/identifiers.h"
#include "engine/symlevel/definitions.h"
#include "engine/terms.h"
#include "engine/typeinfo_manager.h"
#include "interpreter/function_handle.h"
#include "runtimesupport/runtime.h"
#include "utils/iterators.h"
#include "utils/logger.h"
#include "utils/ostream.h"
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

/// This namespace provides resolution functionality, that accesses engine and symlevel.
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
    /// Runtime type info.
    std::optional<RTSupport::TypeInfo> GetTypeInfo() const;

    CbcTypeKind GetKind() const;

    /// The size of a field of given type.
    std::optional<uint32_t> GetFlatSize() const;

    Type(Engine::Term term, Resolver& resolver) : term(term), resolver(&resolver) {}

    Type(Engine::Term term, Resolver* resolver) : term(term), resolver(resolver) {}

    Engine::Term term;
    Resolver* resolver;
};

struct MethodSignature {
    Resolver* resolver;
    Engine::Term term;

    Type ResType() const;
    Engine::Term::Range Params() const;
    uint32_t ParamCount() const;
};

struct DirectCall {
    struct Compiled {
        uintptr_t funcPtr;
        Interpretation::I2Call i2cAdapter;
    };
    using CallData = std::variant<Compiled, Interpretation::DynamicFunctionHandle*>;

    struct Content {
        Type refType;
        std::string_view name;
        MethodSignature signature;
        CallData data;
    };

    Content* operator->() const { return content; };
    Content* Get() const { return content; };

    DirectCall(Content* content) : content(content) {}

private:
    Content* content;
};

struct VirtualCall {
    struct Content {
        Type refType;
        std::string_view name;
        MethodSignature signature;
        int methodNum;
        int extDefNum;
        bool sret;
    };

    Content* operator->() const { return content; };
    Content* Get() const { return content; };

    VirtualCall(Content* content) : content(content) {}

private:
    Content* content;
};

struct InterfaceCall {
    struct Content {
        Type refType;
        std::string_view name;
        MethodSignature signature;
        int methodNum;
        bool sret;
    };

    Content* operator->() const { return content; };
    Content* Get() const { return content; };

    InterfaceCall(Content* content) : content(content) {}

private:
    Content* content;
};

struct InstanceField {
    struct Content {
        Type refType;
        std::string_view name;
        Type fieldType;
        uint32_t ordinal;
        std::optional<uint32_t> offset;
    };

    Content* operator->() const { return content; };
    Content* Get() const { return content; };

    InstanceField(Content* content) : content(content) {}

private:
    Content* content;
};

struct StaticField {
    struct Content {
        Type refType;
        std::string_view name;
        Type fieldType;
        uintptr_t location;
    };

    Content* operator->() const { return content; };
    Content* Get() const { return content; };

    StaticField(Content* content) : content(content) {}

private:
    Content* content;
};

Stream::Output& operator<<(Stream::Output& stream, Type const& type);
Stream::Output& operator<<(Stream::Output& stream, MethodSignature const& sig);
Stream::Output& operator<<(Stream::Output& stream, DirectCall const& call);
Stream::Output& operator<<(Stream::Output& stream, VirtualCall const& call);
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
struct Resolver {
    Resolver(Engine::Session& session, Engine::Identifier<Symlevel::MethodDefinition> method);

    Type Wrap(Engine::Term term);

    std::optional<Type> Query(Index<Type> id);
    std::optional<DirectCall> Query(Index<DirectCall> id);
    std::optional<VirtualCall> Query(Index<VirtualCall> id);
    std::optional<InterfaceCall> Query(Index<InterfaceCall> id);
    std::optional<InstanceField> Query(Index<InstanceField> id);
    std::optional<StaticField> Query(Index<StaticField> id);

    std::optional<Type> QueryFutureByFunctional(Index<Type> id);

    std::optional<InstanceField> QueryTupleElement(Type refType, uint32_t idx);

    std::string_view QueryString(uint32_t stringOffs);

    void GetFullName(Type type, Stream::Output& stream);
    std::optional<RTSupport::TypeInfo> GetTypeInfo(Type type);
    CbcTypeKind GetKind(Type type);
    std::optional<uint32_t> GetFlatSize(Type type);

    template <typename T> using Cache = std::unordered_map<int, typename T::Content*>;

    Engine::Session& session;

private:
    friend class ResolverProxy;
    Engine::Identifier<Symlevel::MethodDefinition> method;
    uint8_t regionId { 0 };
    Engine::TypeInfoManager& tiManager;
    std::unique_ptr<Engine::FieldLayoutManager> fieldManager;
    Engine::TermManager& termManager;

    Cache<VirtualCall> dynamicCalls;
    Cache<InterfaceCall> interfaceCalls;
    Cache<DirectCall> directCalls;
    Cache<InstanceField> instanceFields;
    Cache<StaticField> staticFields;
};

} // namespace Resolution
