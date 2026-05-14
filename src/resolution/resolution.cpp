#include "resolution.h"
#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/method_table.h"
#include "engine/resolving_output.h"
#include "engine/statics_manager.h"
#include "engine/symlevel/aot_table.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/dependencies.h"
#include "engine/symlevel/io/file_id.h"
#include "engine/symlevel/reader.h"
#include "engine/symlevel/references.h"
#include "engine/symlevel/string.h"
#include "engine/terms.h"
#include "engine/typeinfo_manager.h"
#include "interpreter/function_handle.h"
#include "runtimesupport/adapters.h"
#include "utils/assertion.h"
#include "utils/logger.h"
#include "utils/ostream.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>

/// The implementation of resolver consist of:
/// - cache of id -> handle;
/// - handles, which are represented as lazy evaluated storage of properties;

namespace Resolution {

Stream::Descripted logStream(Stream::cerr, "[resolution] ");
Logging::Logger log(&logStream, Logging::Level::ERROR);

using namespace Engine;

static std::string_view CopyToArena(Session& session, std::string const& str)
{
    auto& allocator = session.Allocator();
    auto size       = str.size();

    if (size > 0) {
        char* data = (char*)allocator.Allocate(size + 1, 1);
        data[size] = 0;
        std::copy(str.begin(), str.end(), data);
        return std::string_view(data, size);
    }
    return std::string_view();
}

struct Resolver::Impl {
    Session& session;
    Identifier<Symlevel::MethodDefinition> method;
    IO::FileId fileId;
    uint8_t regionId = 0; // FIXME

    Impl(Session& session, Identifier<Symlevel::MethodDefinition> method)
        : session(session),
          method(method),
          fileId(method.GetFileId())
    {}

    template <typename T> using Cache = std::unordered_map<int, T*>;

    std::unordered_map<Term, Type*, Term::Hasher> types;
    Cache<VirtualCall> dynamicCalls;
    Cache<InterfaceCall> interfaceCalls;
    Cache<DirectCall> directCalls;
    Cache<InstanceField> instanceFields;
    Cache<StaticField> staticFields;

    Type* GetType(Term term)
    {
        ASSERTION(term.GetKind() != TermKind::UNDEFINED, "Expects only defined terms");
        if (auto it = types.find(term); it != types.end()) {
            return it->second;
        }

        auto type = NewType(term);
        types.insert({ term, type });
        return type;
    }

    Type* NewType(Term term);
};

/// TODO: add diffrent Type implementations.
struct SimpleType : public Type {
    Term term;
    Resolver::Impl& impl;

    SimpleType(Term term, Resolver::Impl& impl) : term(term), impl(impl) {}

    void GetFullName(Stream::Output& stream) const override { term.GetName(impl.session, stream); }

    std::optional<RTSupport::TypeInfo> GetTypeInfo() override
    {
        return TypeInfoManager::Of(impl.session).AcquireTypeInfo(impl.session, term);
    }

    CbcTypeKind GetKind() override
    {
        using TK = TermKind;
        switch (term.GetKind()) {
            case TK::NIL:            return CbcTypeKind::INVALID;
            case TK::VOID:           return CbcTypeKind::VOID;
            case TK::UNIT:           return CbcTypeKind::REC;
            case TK::NOTHING:        return CbcTypeKind::INVALID;
            case TK::BOOLEAN:        return CbcTypeKind::BOOL;
            case TK::I8:             return CbcTypeKind::I8;
            case TK::U8:             return CbcTypeKind::U8;
            case TK::I16:            return CbcTypeKind::I16;
            case TK::U16:            return CbcTypeKind::U16;
            case TK::I32:            return CbcTypeKind::I32;
            case TK::U32:            return CbcTypeKind::U32;
            case TK::UCHAR32:        return CbcTypeKind::U32;
            case TK::I64:            return CbcTypeKind::I64;
            case TK::U64:            return CbcTypeKind::U64;
            case TK::IADDR:          return CbcTypeKind::I64;
            case TK::UADDR:          return CbcTypeKind::U64;
            case TK::BSTRING:        return CbcTypeKind::U64;
            case TK::F16:            return CbcTypeKind::U16;
            case TK::F32:            return CbcTypeKind::F32;
            case TK::F64:            return CbcTypeKind::F64;
            case TK::UNDEFINED:      return CbcTypeKind::INVALID;
            case TK::C_POINTER:      return CbcTypeKind::U64;
            case TK::NULLABLE:       return CbcTypeKind::REF;
            case TK::NON_NULLABLE:   return CbcTypeKind::REF;
            case TK::CANGJIE_ARRAY:  return CbcTypeKind::REF;
            case TK::METHOD:         return CbcTypeKind::INVALID;
            case TK::TYPE:           return CbcTypeKind::REF;
            case TK::AOT_TYPE:       return CbcTypeKind::REF;
            case TK::AOT_REC:        return CbcTypeKind::REC;
            case TK::TYPE_VAR:       return CbcTypeKind::REF;
            case TK::GENERIC_METHOD: return CbcTypeKind::INVALID;
            case TK::LAST:           return CbcTypeKind::INVALID;
        }
    }

    std::optional<int> GetFlatSize() override
    {
        using TK = TermKind;
        switch (term.GetKind()) {
            case TK::VOID:
            case TK::UNIT: return 0;

            case TK::BOOLEAN:
            case TK::I8:
            case TK::U8:      return 1;

            case TK::I16:
            case TK::U16:
            case TK::F16: return 2;

            case TK::I32:
            case TK::U32:
            case TK::UCHAR32:
            case TK::F32:     return 4;

            case TK::I64:
            case TK::U64:
            case TK::IADDR:
            case TK::UADDR:
            case TK::BSTRING:
            case TK::F64:
            case TK::C_POINTER: return 8;

            case TK::NULLABLE:
            case TK::NON_NULLABLE:
            case TK::CANGJIE_ARRAY:
            case TK::AOT_TYPE:
            case TK::TYPE_VAR:      return sizeof(uintptr_t);

            case TK::TYPE: {
                auto ident = TypeTermId(term).GetIdentifier();
                auto rec   = Symlevel::TypeDefinition::Resolve(impl.session, ident).GetFlags().GetTypeKind() ==
                           Symlevel::TypeKind::RECORD;
                if (rec) {
                    auto ti = GetTypeInfo();
                    if (!ti.has_value()) {
                        return std::nullopt;
                    }
                    return RTSupport::MetaInfo::GetTypeSize(*ti);
                } else {
                    return sizeof(uintptr_t);
                }
            }

            case TK::AOT_REC: {
                auto ti = GetTypeInfo();
                if (!ti.has_value()) {
                    return std::nullopt;
                }
                return RTSupport::MetaInfo::GetTypeSize(*ti);
            }

            case TK::NIL:
            case TK::NOTHING:
            case TK::UNDEFINED:
            case TK::METHOD:
            case TK::GENERIC_METHOD:
            case TK::LAST:           return std::nullopt;
        }
    }
};

Type* Resolver::Impl::NewType(Term term) { return session.Allocator().New<SimpleType>(term, *this); }

Resolver::Resolver(Session& session, Identifier<Symlevel::MethodDefinition> method)
    : impl(std::make_unique<Impl>(session, method))
{}

Resolver::Resolver(Resolver&& another) = default;
Resolver::~Resolver()                  = default;

template <typename T> static std::optional<T*> ProbeCache(Index<T> id, Resolver::Impl::Cache<T>& cache)
{
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
    RefIdentifier<Symlevel::MethodReference> identifier;
    bool isResolved;

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

static ResolvedMethodReference ResolveReference(Session& session, RefIdentifier<Symlevel::MethodReference> identifier)
{
    auto parsedRef = Symlevel::MethodReference::Parse(session, identifier);
    auto& manager  = TermManager::Of(session);
    auto refType   = manager.Resolve(session, parsedRef.refType);
    auto name      = Symlevel::String::Parse(session, parsedRef.name);
    auto signature = manager.Resolve(session, parsedRef.methodSig);

    bool isResolved = true;
    if (refType.GetKind() == TermKind::UNDEFINED || signature.GetKind() == TermKind::UNDEFINED) {
        // undef terms would be reported separately
        log.Log(Logging::Level::ERROR, [&](Stream::Output& stream) {
            Stream::ResolvingOutput out(session, stream);
            out << "Failed to parse method reference " << identifier << Stream::endl;
        });
        isResolved = false;
    }

    return { refType, name, signature, identifier, isResolved };
}

template <typename Call> static ResolvedMethodReference ResolveReference(Resolver::Impl& resolver, Index<Call> index)
{
    auto refId = Symlevel::RefId<Symlevel::MethodReference>(resolver.regionId, index.GetValue());
    auto ident = RefIdentifier<Symlevel::MethodReference>(refId, resolver.fileId);
    return ResolveReference(resolver.session, ident);
}

static ResolvedFieldReference ResolveReference(Session& session, RefIdentifier<Symlevel::FieldReference> identifier)
{
    auto parsedRef = Symlevel::FieldReference::Parse(session, identifier);
    auto& manager  = TermManager::Of(session);
    auto refType   = manager.Resolve(session, parsedRef.refType);
    auto name      = Symlevel::String::Parse(session, parsedRef.name);
    auto fieldType = manager.Resolve(session, parsedRef.fieldType);
    return { refType, name, fieldType, parsedRef.isRecord };
}

MethodSignature ConstructSignature(Resolver::Impl& resolver, ResolvedMethodReference& ref)
{
    auto signature = ref.signature;
    ASSERTION(signature.GetLength() > 0, "method signature encoding was incorrect");

    auto paramLength = signature.GetLength() - 1;
    auto retTypeIdx  = paramLength;

    std::vector<Type*> params;
    params.reserve(paramLength);
    for (int i = 0; i < paramLength; i++) {
        params.push_back(resolver.GetType(signature.Subterm(i)));
    }
    return {
        .params  = std::move(params),
        .resType = resolver.GetType(signature.Subterm(retTypeIdx)),
    };
}

static std::optional<VirtualCall> ResolveCbcCall(Resolver::Impl& resolver, ResolvedMethodReference& ref)
{
    auto& manager = MethodTableManager::Of(resolver.session);
    auto optMT    = manager.GetMethodTable(resolver.session, ref.refType);
    auto refType  = resolver.GetType(ref.refType);

    if (!optMT.has_value()) {
        return std::nullopt;
    }
    auto mt = *optMT;

    MethodTable::Reference mtRef = {
        .name      = ref.name,
        .signature = ref.signature,
    };
    auto resolved = mt->Resolve(resolver.session, mtRef);

    if (!resolved.has_value()) {
        log.Log(Logging::Level::ERROR, [&](Stream::Output& stream) {
            stream << "Failed to resolve method " << ref.GetFullName(resolver.session) << Stream::endl;
        });
        return std::nullopt;
    }

    auto sig = ConstructSignature(resolver, ref);
    return VirtualCall { refType, ref.name, std::move(sig), resolved->methodNum, resolved->subTableNum };
}

static std::optional<InterfaceCall> ResolveCall(Resolver::Impl& resolver, Index<InterfaceCall> id)
{
    auto [file, raf] = resolver.session.File(resolver.fileId);
    auto ref         = ResolveReference(resolver, id);
    if (!ref.isResolved) {
        return std::nullopt;
    }
    auto refType = resolver.GetType(ref.refType);

    switch (ref.refType.GetKind()) {
        case TermKind::TYPE: {
            auto call = ResolveCbcCall(resolver, ref);
            if (call.has_value()) {
                return InterfaceCall { call->refType, call->name, std::move(call->signature), call->methodNum };
            }
            return std::nullopt;
        }

        case TermKind::AOT_TYPE: {
            /// FIXME: interface calls
            auto data = file.GetInterfaceCallAotTable().GetData(resolver.session, ref.identifier.GetIndex());
            auto sig  = ConstructSignature(resolver, ref);
            return InterfaceCall { refType, ref.name, std::move(sig), data.inum };
        }

        default: {
            log.Stream(Logging::Level::FATAL) << "Unexpected ref type in reference " << id.GetValue();
            return std::nullopt;
        }
    }
}

static std::optional<VirtualCall> ResolveCall(Resolver::Impl& resolver, Index<VirtualCall> id)
{
    auto [file, raf] = resolver.session.File(resolver.fileId);
    auto ref         = ResolveReference(resolver, id);
    if (!ref.isResolved) {
        return std::nullopt;
    }
    auto refType = resolver.GetType(ref.refType);

    switch (ref.refType.GetKind()) {
        case TermKind::TYPE: {
            return ResolveCbcCall(resolver, ref);
        }

        case TermKind::AOT_TYPE: {
            /// FIXME: interface calls
            auto data = file.GetVirtualCallAotTable().GetData(resolver.session, ref.identifier.GetIndex());
            auto sig  = ConstructSignature(resolver, ref);
            return VirtualCall { refType, ref.name, std::move(sig), data.methodNum, data.extDefNum };
        }

        default: {
            log.Stream(Logging::Level::FATAL) << "Unexpected ref type in reference " << id.GetValue();
            return std::nullopt;
        }
    }
}

static std::optional<DirectCall> ResolveCall(Resolver::Impl& resolver, Index<DirectCall> id)
{
    auto [file, raf] = resolver.session.File(resolver.fileId);
    auto ref         = ResolveReference(resolver, id);
    if (!ref.isResolved) {
        return std::nullopt;
    }
    auto refType = resolver.GetType(ref.refType);

    switch (ref.refType.GetKind()) {
        case TermKind::TYPE: {
            auto termIdent = TypeTermId(ref.refType);
            auto type      = Symlevel::TypeDefinition::Resolve(resolver.session, termIdent.GetIdentifier());

            // FIXME: search in hierarchy
            auto method = [&]() -> std::optional<Identifier<Symlevel::MethodDefinition>> {
                auto methods = type.GetMethods().FindMethods(resolver.session, ref.name);

                for (auto m : methods) {
                    auto def = Symlevel::Reader::Read(resolver.session, m);
                    auto sig = TermManager::Resolve(resolver.session, def.Signature());
                    if (sig == ref.signature) {
                        return m;
                    }
                }
                log.Log(Logging::Level::ERROR, [&](Stream::Output& stream) {
                    stream << "Failed to resolve method " << ref.GetFullName(resolver.session) << Stream::endl;
                });
                return std::nullopt;
            }();

            if (!method.has_value()) {
                return std::nullopt;
            }

            auto fuh = Interpretation::FunctionHandleManager::Of(resolver.session)
                           .AcquireTagged(resolver.session, method.value());
            auto sig = ConstructSignature(resolver, ref);

            // TODO: simplify
            if (auto* staticFuh = std::get_if<Interpretation::StaticFunctionHandle*>(&fuh)) {
                auto fuh                  = *staticFuh;
                DirectCall::CallData data = DirectCall::Compiled {
                    .funcPtr    = reinterpret_cast<uintptr_t>(fuh->function),
                    .i2cAdapter = fuh->base.i2call,
                };
                return DirectCall { refType, ref.name, std::move(sig), data };
            } else {
                auto dynFuh               = std::get<Interpretation::DynamicFunctionHandle*>(fuh);
                DirectCall::CallData data = dynFuh;
                return DirectCall { refType, ref.name, std::move(sig), data };
            }
        }

        case TermKind::AOT_TYPE: {
            auto data = file.GetDirectCallAotTable().GetData(resolver.session, ref.identifier.GetIndex());

            auto linkageName = Symlevel::String::Parse(resolver.session, data.linkangeName);
            auto funcPtr     = file.GetDependencies().FindTarget(linkageName);

            if (!funcPtr) {
                log.Log(Logging::Level::FATAL, [linkageName](Stream::Output& stream) {
                    stream << "not found location of static field: " << linkageName << Stream::endl;
                });
                return std::nullopt;
            }

            auto sig = ConstructSignature(resolver, ref);

            /// FIXME: choose proper adapter based on signature (abi)
            DirectCall::CallData callData = DirectCall::Compiled {
                .funcPtr    = reinterpret_cast<uintptr_t>(funcPtr),
                .i2cAdapter = RTSupport::Adapters::GenericI2CCallInstance(),
            };

            return DirectCall { refType, ref.name, std::move(sig), callData };
        }

        default: {
            log.Stream(Logging::Level::FATAL) << "Unexpected ref type in reference " << id.GetValue();
            return std::nullopt;
        }
    }
}

std::optional<VirtualCall const*> Resolver::Query(Index<VirtualCall> id)
{
    if (auto opt = ProbeCache(id, impl->dynamicCalls); opt.has_value()) {
        return opt.value();
    }
    if (auto opt = ResolveCall(*impl, id); opt.has_value()) {
        auto res = impl->session.Allocator().New<VirtualCall>(opt.value());
        impl->dynamicCalls.insert({ id.GetValue(), res });
        return res;
    }
    return std::nullopt;
}

std::optional<InterfaceCall const*> Resolver::Query(Index<InterfaceCall> id)
{
    if (auto opt = ProbeCache(id, impl->interfaceCalls); opt.has_value()) {
        return opt.value();
    }
    if (auto opt = ResolveCall(*impl, id); opt.has_value()) {
        auto res = impl->session.Allocator().New<InterfaceCall>(opt.value());
        impl->interfaceCalls.insert({ id.GetValue(), res });
        return res;
    }
    return std::nullopt;
}

std::optional<DirectCall const*> Resolver::Query(Index<DirectCall> id)
{
    if (auto opt = ProbeCache(id, impl->directCalls); opt.has_value()) {
        return opt.value();
    }
    if (auto opt = ResolveCall(*impl, id); opt.has_value()) {
        auto res = impl->session.Allocator().New<DirectCall>(opt.value());
        impl->directCalls.insert({ id.GetValue(), res });
        return res;
    }
    return std::nullopt;
}

template <typename Field> std::optional<Field> ResolveField(Resolver::Impl& resolver, Index<Field> id)
{
    auto fileId = resolver.method.GetFileId();

    auto refId = Symlevel::RefId<Symlevel::FieldReference>(resolver.regionId, id.GetValue());
    auto ident = RefIdentifier<Symlevel::FieldReference>(refId, fileId);
    auto ref   = ResolveReference(resolver.session, ident);

    if (ref.refType.GetKind() == TermKind::UNDEFINED || ref.fieldType.GetKind() == TermKind::UNDEFINED) {
        // undef terms would be reported separately
        log.Stream(Logging::Level::ERROR) << "Failed to parse field reference " << id.GetValue();
        return std::nullopt;
    }

    auto [file, raf] = resolver.session.File(fileId);
    auto refType     = resolver.GetType(ref.refType);
    auto fieldType   = resolver.GetType(ref.fieldType);

    switch (ref.refType.GetKind()) {
        case TermKind::AOT_REC:
        case TermKind::AOT_TYPE: {
            ASSERTION(ref.fieldType.GetKind() != TermKind::TYPE, "aot types cannot have fields of cbc type");
            if constexpr (std::is_same_v<Field, InstanceField>) {
                auto data = file.GetInstanceFieldAotTable().GetData(resolver.session, refId);
                int offset =
                    RTSupport::Execution::GetFieldOffset(refType->GetTypeInfo().value(), data.ordinal, !ref.isRecord);
                return InstanceField { refType, ref.name, fieldType, data.ordinal, offset };
            } else {
                static_assert(std::is_same_v<Field, StaticField>);
                auto data        = file.GetStaticFieldAotTable().GetData(resolver.session, refId);
                auto linkageName = Symlevel::String::Parse(resolver.session, data.linkangeName);
                auto location    = file.GetDependencies().FindTarget(linkageName);
                if (!location) {
                    log.Log(Logging::Level::FATAL, [linkageName](Stream::Output& stream) {
                        stream << "not found location of static field: " << linkageName << Stream::endl;
                    });
                    return std::nullopt;
                }
                return StaticField { refType, ref.name, fieldType, reinterpret_cast<uintptr_t>(location) };
            }
        }
        case TermKind::TYPE: {
            if constexpr (std::is_same_v<Field, InstanceField>) {
                FATAL("Not supported yet");
                return std::nullopt;
            } else {
                static_assert(std::is_same_v<Field, StaticField>);

                auto typeDefIdent = TypeTermId(ref.refType).GetIdentifier();
                auto typeDef      = Symlevel::TypeDefinition::Resolve(resolver.session, typeDefIdent);

                auto fieldDefIdentOpt = typeDef.GetFields().FindField(resolver.session, ref.name);
                if (!fieldDefIdentOpt.has_value()) {
                    log.Stream(Logging::Level::ERROR)
                        << "Field definition search failed " << id.GetValue() << Stream::endl;
                    return std::nullopt;
                }

                auto fieldDef        = Symlevel::FieldDefinition::Resolve(resolver.session, fieldDefIdentOpt.value());
                auto actualFieldType = TermManager::Resolve(resolver.session, fieldDef.FieldType());
                if (ref.fieldType != actualFieldType) {
                    log.Stream(Logging::Level::ERROR)
                        << "Field type mismatch expected:  " << ref.fieldType.GetName(resolver.session)
                        << ", actual: " << actualFieldType.GetName(resolver.session) << Stream::endl;
                    return std::nullopt;
                }

                uintptr_t location = StaticsManager::Of(resolver.session)
                                         .GetLocation(resolver.session, typeDefIdent, fieldDefIdentOpt.value());

                return StaticField { refType, ref.name, fieldType, location };
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
        impl->instanceFields.insert({ id.GetValue(), res });
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
        impl->staticFields.insert({ id.GetValue(), res });
        return res;
    }
    return std::nullopt;
}

std::optional<Type*> Resolver::Query(Index<Type> id)
{
    // terms are being cached on different level
    auto refId = Symlevel::RefId<Term>(impl->regionId, id.GetValue());
    auto ident = RefIdentifier<Term>(refId, impl->method.GetFileId());
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
    stream << *call.refType << '.' << call.name << call.signature;
    return stream;
}

Stream::Output& operator<<(Stream::Output& stream, VirtualCall const& call)
{
    stream << *call.refType << '.' << call.name << call.signature;
    return stream;
}

Stream::Output& operator<<(Stream::Output& stream, InstanceField const& field)
{
    stream << *field.refType << '.' << field.name << '.' << *field.fieldType;
    return stream;
}

Stream::Output& operator<<(Stream::Output& stream, StaticField const& field)
{
    stream << *field.refType << '.' << field.name << '.' << *field.fieldType;
    return stream;
}

} // namespace Resolution
