#include "resolution.h"
#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/references.h"
#include "engine/symlevel/string.h"
#include "engine/symlevel/dependencies.h"
#include "engine/terms.h"
#include "engine/symlevel/aot_table.h"
#include "engine/typeinfo_manager.h"
#include "interpreter/function_handle.h"
#include "runtimesupport/adapters.h"
#include "utils/assertion.h"
#include "utils/logger.h"
#include "utils/ostream.h"
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>

/// The implementation of resolver consist of:
/// - cache of id -> handle;
/// - handles, which are represented as lazy evaluated storage of properties;

namespace Resolution {

Stream::Descripted logStream(Stream::cerr, "[resolution] ");
Logging::Logger log(&logStream, Logging::Level::NONE);

using namespace Engine;

static std::string_view CopyToArena(Session& session, std::string const& str)
{
    auto& allocator = session.Allocator();
    auto size = str.size();

    if (size > 0) {
        char* data = (char*) allocator.Allocate(size + 1, 1);
        data[size] = 0;
        std::copy(str.begin(), str.end(), data);
        return std::string_view(data, size);
    }
    return std::string_view();
}

struct Resolver::Impl {
    Session& session;
    Identifier<Symlevel::MethodDefinition> method;

    Impl(Session& session, Identifier<Symlevel::MethodDefinition> method)
        : session(session), method(method)
    {}

    template <typename T>
    using Cache = std::unordered_map<int, T*>;

    std::unordered_map<Term, Type*, Term::Hasher> types;
    Cache<DynamicCall> dynamicCalls;
    Cache<DirectCall> directCalls;
    Cache<InstanceField> instanceFields;
    Cache<StaticField> staticFields;

    Type* GetType(Term term) {
        ASSERTION(term.GetKind() != TemplateKind::UNDEFINED, "Expects only defined terms");
        if (auto it = types.find(term); it != types.end()) {
            return it->second;
        }

        auto type = NewType(term);
        types.insert({term, type});
        return type;
    }

    Type* NewType(Term term);
};

/// TODO: add diffrent Type implementations.
struct SimpleType : public Type {
    Term term;
    Resolver::Impl& impl;

    SimpleType(Term term, Resolver::Impl& impl) : term(term), impl(impl) {}

    void GetFullName(Stream::Output& stream) const override {
        term.GetName(impl.session, stream);
    }

    std::optional<RTSupport::TypeInfo> GetTypeInfo() override {
        return TypeInfoManager::Of(impl.session).AcquireTypeInfo(impl.session, term);
    }

    CbcTypeKind GetKind() override {
        using TK = TemplateKind;
        switch (term.GetKind()) {
            case TK::NIL: return CbcTypeKind::INVALID;
            case TK::VOID: return CbcTypeKind::VOID;
            case TK::UNIT: return CbcTypeKind::REC;
            case TK::NOTHING: return CbcTypeKind::INVALID;
            case TK::BOOLEAN: return CbcTypeKind::BOOL;
            case TK::I8: return CbcTypeKind::I8;
            case TK::U8: return CbcTypeKind::U8;
            case TK::I16: return CbcTypeKind::I16;
            case TK::U16: return CbcTypeKind::U16;
            case TK::I32: return CbcTypeKind::I32;
            case TK::U32: return CbcTypeKind::U32;
            case TK::UCHAR32: return CbcTypeKind::U32;
            case TK::I64: return CbcTypeKind::I64;
            case TK::U64: return CbcTypeKind::U64;
            case TK::IADDR: return CbcTypeKind::I64;
            case TK::UADDR: return CbcTypeKind::U64;
            case TK::BSTRING: return CbcTypeKind::U64;
            case TK::F16: return CbcTypeKind::U16;
            case TK::F32: return CbcTypeKind::F32;
            case TK::F64: return CbcTypeKind::F64;
            case TK::UNDEFINED: return CbcTypeKind::INVALID;
            case TK::C_POINTER: return CbcTypeKind::U64;
            case TK::NULLABLE: return CbcTypeKind::REF;
            case TK::NON_NULLABLE: return CbcTypeKind::REF;
            case TK::CANGJIE_ARRAY: return CbcTypeKind::REF;
            case TK::METHOD: return CbcTypeKind::INVALID;
            case TK::TYPE: return CbcTypeKind::REF; // FIXME
            case TK::AOT_TYPE: return CbcTypeKind::REF; // FIXME
            case TK::TYPE_VAR: return CbcTypeKind::REF;
            case TK::GENERIC_METHOD: return CbcTypeKind::INVALID;
            case TK::LAST: return CbcTypeKind::INVALID;
        }
    }
};

Type* Resolver::Impl::NewType(Term term) {
    return session.Allocator().New<SimpleType>(term, *this);
}

Resolver::Resolver(Session& session, Identifier<Symlevel::MethodDefinition> method)
    : impl(std::make_unique<Impl>(session, method))
{}

Resolver::Resolver(Resolver&& another) = default;
Resolver::~Resolver() = default;

template <typename T>
static std::optional<T*> ProbeCache(Index<T> id, Resolver::Impl::Cache<T>& cache) {
    auto it = cache.find(id.GetValue());
    if (it != cache.end()) {
        return it->second;
    }
    return std::nullopt;
}

struct ResolvedMethodReference {
    Term refType;
    std::string_view name;
    Term signature;

    std::string GetFullName(Session& session)
    {
        Stream::StringBuffer buf;
        buf << refType.GetName(session) << '.' << name << signature.GetName(session);
        return buf.ToString();
    }
};

struct ResolvedFieldReference {
    Term refType;
    std::string_view name;
    Term fieldType;
    bool isRecord;

    std::string GetFullName(Session& session)
    {
        Stream::StringBuffer buf;
        buf << refType.GetName(session) << '.' << name << fieldType.GetName(session);
        return buf.ToString();
    }
};

static ResolvedMethodReference ResolveReference(Session& session, IndexIdentifier<Symlevel::MethodReference> identifier) {
    auto parsedRef = Symlevel::MethodReference::Parse(session, identifier);
    auto& manager = TermManager::Of(session);
    auto refType = manager.Resolve(session, parsedRef.refType);
    auto name = Symlevel::String::Parse(session, parsedRef.name);
    auto signature = manager.Resolve(session, parsedRef.methodSig);
    return { refType, name, signature };
}

static ResolvedFieldReference ResolveReference(Session& session, IndexIdentifier<Symlevel::FieldReference> identifier) {
    auto parsedRef = Symlevel::FieldReference::Parse(session, identifier);
    auto& manager = TermManager::Of(session);
    auto refType = manager.Resolve(session, parsedRef.refType);
    auto name = Symlevel::String::Parse(session, parsedRef.name);
    auto fieldType = manager.Resolve(session, parsedRef.fieldType);
    return { refType, name, fieldType, parsedRef.isRecord };
}

MethodSignature ConstructSignature(Resolver::Impl& resolver, ResolvedMethodReference& ref) {
    // FIXME
    return {};
}

template <typename Call> // FIXME: split?
std::optional<Call> ResolveCall(Resolver::Impl& resolver, Index<Call> id) {
    auto& session = resolver.session;
    auto fileId = resolver.method.GetFileId();

    // FIXME: region num
    auto refId = Symlevel::Index<Symlevel::MethodReference>(0, id.GetValue());
    auto ident = IndexIdentifier<Symlevel::MethodReference>(refId, fileId);
    auto ref   = ResolveReference(session, ident);

    if (ref.refType.GetKind() == TemplateKind::UNDEFINED || ref.signature.GetKind() == TemplateKind::UNDEFINED) {
        // undef terms would be reported separately
        log.Log(Logging::Level::ERROR, [id](Stream::Output& stream) {
            stream << "Failed to parse method reference " << id.GetValue() << Stream::endl;
        });
        return std::nullopt;
    }

    auto [file, raf] = resolver.session.File(fileId);
    auto refType = resolver.GetType(ref.refType);

    switch (ref.refType.GetKind()) {
        case TemplateKind::TYPE: {
            if constexpr (std::is_same_v<Call, DynamicCall>) {
                // FIXME: method resolution for dynamic calls.
                FATAL("Not supported yet: cbc dyn calls");
            } else {
                static_assert(std::is_same_v<Call, DirectCall>);

                auto termIdent = ref.refType.GetIdentifier().AsTypeIdent();
                auto type = Symlevel::TypeDefinition::Resolve(session, termIdent.GetIdentifier());
                auto methods = type.GetMethodIndex().FindMethods(session, ref.name);

                if (methods.size() != 1) {
                    // FIXME: proper resolution
                    log.Log(Logging::Level::ERROR, [&ref, &session](Stream::Output& stream) {
                        stream << "Failed to resolve method (note, overloading not supported yet) " << ref.GetFullName(session) << Stream::endl;
                    });
                    return std::nullopt;
                }

                auto fuh = Interpretation::FunctionHandleManager::Of(session).AcquireTagged(session, methods[0]);
                auto sig = ConstructSignature(resolver, ref);

                if (auto* staticFuh = std::get_if<Interpretation::StaticFunctionHandle*>(&fuh)) {
                    auto fuh = *staticFuh;
                    DirectCall::CallData data = DirectCall::Compiled {
                        .funcPtr = reinterpret_cast<uintptr_t>(fuh->function),
                        .i2cAdapter = fuh->base.i2call,
                    };
                    return DirectCall {refType, ref.name, std::move(sig), data};
                } else {
                    auto dynFuh = std::get<Interpretation::DynamicFunctionHandle*>(fuh);
                    DirectCall::CallData data = dynFuh;
                    return DirectCall {refType, ref.name, std::move(sig), data};
                }
            }
        }

        case TemplateKind::AOT_TYPE: {
            if constexpr (std::is_same_v<Call, DynamicCall>) {
                /// FIXME: interface calls
                auto data = file.GetVirtualCallAotTable().GetData(session, refId);
                auto sig = ConstructSignature(resolver, ref);
                return DynamicCall { refType, ref.name, std::move(sig), data.methodNum, data.extDefNum };
            } else {
                static_assert(std::is_same_v<Call, DirectCall>);

                auto data = file.GetDirectCallAotTable().GetData(session, refId);

                auto linkageName = Symlevel::String::Parse(resolver.session, data.linkangeName);
                auto funcPtr = file.GetDependencies().FindTarget(linkageName);

                if (!funcPtr) {
                    log.Log(Logging::Level::FATAL, [linkageName](Stream::Output& stream) {
                        stream << "not found location of static field: " << linkageName << Stream::endl;
                    });
                    return std::nullopt;
                }

                auto sig = ConstructSignature(resolver, ref);

                /// FIXME: choose proper adapter
                DirectCall::CallData callData = DirectCall::Compiled {
                    .funcPtr = reinterpret_cast<uintptr_t>(funcPtr),
                    .i2cAdapter = RTSupport::Adapters::GenericI2CCallInstance(),
                };

                return DirectCall {refType, ref.name, std::move(sig), callData};
            }
        }

        default: {
            log.Stream(Logging::Level::FATAL) << "Unexpected ref type in reference " << id.GetValue();
            return std::nullopt;
        }
    }
}

std::optional<DynamicCall const*> Resolver::Query(Index<DynamicCall> id) {
    if (auto opt = ProbeCache(id, impl->dynamicCalls); opt.has_value()) {
        return opt.value();
    }
    if (auto opt = ResolveCall(*impl, id); opt.has_value()) {
        auto res = impl->session.Allocator().New<DynamicCall>(opt.value());
        impl->dynamicCalls.insert({id.GetValue(), res});
        return res;
    }
    return std::nullopt;
}

std::optional<DirectCall const*> Resolver::Query(Index<DirectCall> id) {
    if (auto opt = ProbeCache(id, impl->directCalls); opt.has_value()) {
        return opt.value();
    }
    if (auto opt = ResolveCall(*impl, id); opt.has_value()) {
        auto res = impl->session.Allocator().New<DirectCall>(opt.value());
        impl->directCalls.insert({id.GetValue(), res});
        return res;
    }
    return std::nullopt;
}

template <typename Field>
std::optional<Field> ResolveField(Resolver::Impl& resolver, Index<Field> id)
{
    auto fileId = resolver.method.GetFileId();

    // FIXME: region num
    auto refId = Symlevel::Index<Symlevel::FieldReference>(0, id.GetValue());
    auto ident = IndexIdentifier<Symlevel::FieldReference>(refId, fileId);
    auto ref   = ResolveReference(resolver.session, ident);

    if (ref.refType.GetKind() == TemplateKind::UNDEFINED || ref.fieldType.GetKind() == TemplateKind::UNDEFINED) {
        // undef terms would be reported separately
        log.Stream(Logging::Level::ERROR) << "Failed to parse field reference " << id.GetValue();
        return std::nullopt;
    }

    auto [file, raf] = resolver.session.File(fileId);
    auto refType = resolver.GetType(ref.refType);
    auto fieldType = resolver.GetType(ref.fieldType);

    switch (ref.refType.GetKind()) {
        case TemplateKind::AOT_TYPE: {
            ASSERTION(ref.fieldType.GetKind() != TemplateKind::TYPE, "aot types cannot have fields of cbc type");
            if constexpr (std::is_same_v<Field, InstanceField>) {
                auto data = file.GetInstanceFieldAotTable().GetData(resolver.session, refId);
                int offset = RTSupport::Execution::GetFieldOffset(refType->GetTypeInfo().value(), data.ordinal, !ref.isRecord);
                return InstanceField {refType, ref.name, fieldType, data.ordinal, offset};
            } else {
                static_assert(std::is_same_v<Field, StaticField>);
                auto data = file.GetStaticFieldAotTable().GetData(resolver.session, refId);
                auto linkageName = Symlevel::String::Parse(resolver.session, data.linkangeName);
                auto location = file.GetDependencies().FindTarget(linkageName);
                if (!location) {
                    log.Log(Logging::Level::FATAL, [linkageName](Stream::Output& stream) {
                        stream << "not found location of static field: " << linkageName << Stream::endl;
                    });
                    return std::nullopt;
                }
                return StaticField { refType, ref.name, fieldType, reinterpret_cast<uintptr_t>(location) };
            }
        }
        default: {
            FATAL("Not supported yet %d", ref.refType.GetKind());
            return std::nullopt;
        }
    }
}

std::optional<InstanceField const*> Resolver::Query(Index<InstanceField> id)
{
    if (auto opt = ProbeCache(id, impl->instanceFields); opt.has_value()) {
        return opt.value();
    }
    if (auto opt = ResolveField(*impl, id); opt.has_value()) {
        auto res = impl->session.Allocator().New<InstanceField>(opt.value());
        impl->instanceFields.insert({id.GetValue(), res});
        return res;
    }
    return std::nullopt;
}

std::optional<StaticField const*> Resolver::Query(Index<StaticField> id)
{
    if (auto opt = ProbeCache(id, impl->staticFields); opt.has_value()) {
        return opt.value();
    }
    if (auto opt = ResolveField(*impl, id); opt.has_value()) {
        auto res = impl->session.Allocator().New<StaticField>(opt.value());
        impl->staticFields.insert({id.GetValue(), res});
        return res;
    }
    return std::nullopt;
}

std::optional<Type*> Resolver::Query(Index<Type> id)
{
    // terms are being cached on different level
    auto refId = Symlevel::Index<Symlevel::Term>(0, id.GetValue());
    auto ident = IndexIdentifier<Symlevel::Term>(refId, impl->method.GetFileId());
    auto term  = TermManager::Of(impl->session).Resolve(impl->session, ident);
    return impl->GetType(term);
}

Stream::Output& operator<<(Stream::Output& stream, Type const& type)
{
    type.GetFullName(stream);
    return stream;
}

Stream::Output& operator<<(Stream::Output& stream, MethodSignature const& sig)
{
    stream << "(";
    auto sep = " ";
    for (auto& t : sig.params) {
        stream << sep << *t;
        sep = ", ";
    }
    stream << ")";
    stream << *sig.resType;
    return stream;
}

Stream::Output& operator<<(Stream::Output& stream, DirectCall const& call)
{
    stream << call.refType << '.' << call.name << '.' << call.signature;
    return stream;
}

Stream::Output& operator<<(Stream::Output& stream, DynamicCall const& call)
{
    stream << call.refType << '.' << call.name << '.' << call.signature;
    return stream;
}

Stream::Output& operator<<(Stream::Output& stream, InstanceField const& field)
{
    stream << field.refType << '.' << field.name << '.' << field.fieldType;
    return stream;
}

Stream::Output& operator<<(Stream::Output& stream, StaticField const& field)
{
    stream << field.refType << '.' << field.name << '.' << field.fieldType;
    return stream;
}

}
