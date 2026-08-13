#include "resolution.h"
#include "engine/decode/decoder.h"
#include "engine/engine.h"
#include "engine/field_layout.h"
#include "engine/identifiers.h"
#include "engine/method_table.h"
#include "engine/resolving_output.h"
#include "engine/statics_manager.h"
#include "engine/symlevel/aot_table.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/dependencies.h"
#include "engine/symlevel/flags.h"
#include "engine/symlevel/io/file_id.h"
#include "engine/symlevel/reader.h"
#include "engine/symlevel/references.h"
#include "engine/symlevel/string.h"
#include "engine/terms.h"
#include "engine/typeinfo_manager.h"
#include "interpreter/function_handle.h"
#include "runtimesupport/adapters.h"
#include "runtimesupport/runtime.h"
#include "utils/assertion.h"
#include "utils/iterators.h"
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

std::optional<RTSupport::TypeInfo> Type::GetTypeInfo() const { return resolver->GetTypeInfo(*this); }

std::optional<uint32_t> Type::GetFlatSize() const { return resolver->GetFlatSize(*this); }

CbcTypeKind Type::GetKind() const { return resolver->GetKind(*this); }

CbcTypeKind Resolver::GetKind(Type type)
{
    if (type.term.IsReference()) {
        return CbcTypeKind::REF;
    } else if (type.term.IsRecord()) {
        return CbcTypeKind::REC;
    }
    using TK  = TermKind;
    auto term = type.term;
    switch (term.GetKind()) {
        case TK::NIL:            return CbcTypeKind::INVALID;
        case TK::VOID:           return CbcTypeKind::VOID;
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
        case TK::PRIMITIVE_ENUM: {
            auto id             = PrimitiveEnumId(term.GetId());
            auto definition     = Symlevel::Reader::Read(session, id.GetIdentifier());
            auto underlyingType = TermManager::Resolve(session, definition.GetEnumType());
            ClassSubstitution substitution(session, term);
            return GetKind(Wrap(substitution.Substitute(underlyingType)));
        }
        case TK::LAST: return CbcTypeKind::INVALID;

        default: FATAL("Unexpected %d", term.GetKind());
    }
}

std::optional<uint32_t> Resolver::GetFlatSize(Type type) { return fieldManager->GetFlatSize(type.term); }

Resolver::Resolver(Session& session, Identifier<Symlevel::MethodDefinition> method)
    : session(session),
      method(method),
      regionId(0),
      tiManager(TypeInfoManager::Of(session)),
      fieldManager(FieldLayoutManager::New(session, tiManager)),
      termManager(TermManager::Of(session))
{}

struct ResolvedMethodReference {
    Term refType;
    std::string_view name;
    Term signature;
    RefIdentifier<Symlevel::MethodReference> identifier;
    Symlevel::MethodRefFlags flags;
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
    RefIdentifier<Symlevel::FieldReference> identifier;
    bool isRecord;

    std::string GetFullName(Session& session)
    {
        Stream::StringBuffer buf;
        buf << refType.GetName(session) << '.' << name << fieldType.GetName(session);
        return buf.ToString();
    }
};

struct ResolverProxy {
    /// Routine that substitutes type variables with `stub`.
    class StubSubstitution : public Substitution {
    public:
        StubSubstitution(Session& session, Term term) : Substitution(session), stub(term) {}

    protected:
        Term SubstituteClassTv(uint8_t typeVar) override { return stub; }

        Term SubstituteFuncTv(uint8_t typeVar) override { return stub; }

    private:
        Term stub;
    };

    static std::optional<InstanceField::Content> ResolveAotInstanceField(
        Resolver& resolver, ResolvedFieldReference& ref
    )
    {
        auto refType     = resolver.Wrap(ref.refType);
        auto fieldType   = resolver.Wrap(ref.fieldType);
        auto [file, raf] = resolver.session.File(resolver.method.GetFileId());
        auto data        = file.GetInstanceFieldAotTable().GetData(resolver.session, ref.identifier.GetIndex());

        auto refTypeFlags                     = refType.term.Flags();
        std::optional<uint32_t> offset        = std::nullopt;
        std::optional<RTSupport::TypeInfo> ti = std::nullopt;

        if (refTypeFlags.isGeneric && refTypeFlags.isFixedSize) {
            // We can not query TypeInfo for generic type to access its field.
            // Since refType is fixed size type, we can query TI of any instantiation
            // of given type which will have the exact same field layout generic one.
            StubSubstitution sub(resolver.session, Term::Predefined(TermKind::I64));
            auto concrete = sub.Substitute(refType.term);
            ti            = resolver.tiManager.AcquireTypeInfo(resolver.session, concrete);
        } else if (!refTypeFlags.isGeneric) {
            ti = refType.GetTypeInfo();
        }

        if (ti) { // for generic instance fields
            offset = RTSupport::Execution::GetFieldOffset(*ti, data.ordinal, ref.refType.IsReference());
        }
        return InstanceField::Content { refType, ref.name, fieldType, data.ordinal, offset };
    }

    static std::optional<StaticField::Content> ResolveAotStaticField(Resolver& resolver, ResolvedFieldReference& ref)
    {
        auto refType     = resolver.Wrap(ref.refType);
        auto fieldType   = resolver.Wrap(ref.fieldType);
        auto [file, raf] = resolver.session.File(resolver.method.GetFileId());
        auto data        = file.GetStaticFieldAotTable().GetData(resolver.session, ref.identifier.GetIndex());
        auto linkageName = Symlevel::String::Parse(resolver.session, data.linkangeName);
        auto location    = file.GetDependencies().FindTarget(linkageName);
        if (!location) {
            log.Log(Logging::Level::FATAL, [linkageName](Stream::Output& stream) {
                stream << "not found location of static field: " << linkageName << Stream::endl;
            });
            return std::nullopt;
        }
        return StaticField::Content { refType, ref.name, fieldType, reinterpret_cast<uintptr_t>(location) };
    }

    template <typename Field>
    static std::optional<typename Field::Content> ResolveField(Resolver& resolver, Index<Field> id)
    {
        auto fileId = resolver.method.GetFileId();

        auto ident = RefIdentifier<Symlevel::FieldReference>(id, fileId);
        auto ref   = ResolveReference(resolver.session, resolver.termManager, ident);

        if (ref.refType.GetKind() == TermKind::UNDEFINED || ref.fieldType.GetKind() == TermKind::UNDEFINED) {
            // undef terms would be reported separately
            log.Stream(Logging::Level::ERROR) << "Failed to parse field reference " << id.GetValue() << Stream::endl;
            return std::nullopt;
        }

        auto [file, raf] = resolver.session.File(fileId);
        auto refType     = resolver.Wrap(ref.refType);
        auto fieldType   = resolver.Wrap(ref.fieldType);

        switch (ref.refType.GetKind()) {
            case TermKind::AOT_TYPE: {
                if constexpr (std::is_same_v<Field, InstanceField>) {
                    return ResolveAotInstanceField(resolver, ref);
                } else {
                    static_assert(std::is_same_v<Field, StaticField>);
                    return ResolveAotStaticField(resolver, ref);
                }
            }
            case TermKind::TYPE: {
                if constexpr (std::is_same_v<Field, InstanceField>) {
                    if (ref.refType.IsAotPromoted()) {
                        return ResolveAotInstanceField(resolver, ref);
                    }
                    auto optlayout = resolver.fieldManager->GetLayout(ref.refType);
                    if (!optlayout.has_value()) {
                        return std::nullopt;
                    }
                    auto layout = *optlayout;

                    uint32_t ordinal = 0;
                    auto optoffset   = [&]() {
                        std::optional<uint32_t> offset {};
                        for (auto& field : layout->fields) {
                            if (!field.definition)
                                continue;
                            auto def  = Symlevel::Reader::Read(resolver.session, *field.definition);
                            auto name = Symlevel::Reader::Read(resolver.session, def.GetName());
                            if (field.fieldType == ref.fieldType && name.compare(ref.name) == 0) {
                                offset = field.offset;
                                break;
                            }
                            ordinal++;
                        }
                        return offset;
                    }();
                    if (optoffset.has_value()) {
                        auto offset  = *optoffset;
                        offset      += (ref.refType.IsReference() ? RTSupport::MetaInfo::ObjectHeaderSize() : 0);
                        optoffset    = offset;
                    }
                    return InstanceField::Content { refType, ref.name, fieldType, ordinal, optoffset };
                } else {
                    if (ref.refType.IsAotPromoted()) {
                        return ResolveAotStaticField(resolver, ref);
                    }
                    static_assert(std::is_same_v<Field, StaticField>);

                    auto typeDefIdent = TypeTermId(ref.refType).GetIdentifier();
                    auto typeDef      = Symlevel::Reader::Read(resolver.session, typeDefIdent);

                    auto fieldDefIdentOpt = resolver.session.Decoder().Find(typeDef.GetFields(), ref.name);
                    if (!fieldDefIdentOpt.has_value()) {
                        log.Stream(Logging::Level::ERROR)
                            << "Field definition search failed " << id.GetValue() << Stream::endl;
                        return std::nullopt;
                    }

                    auto fieldDef        = Symlevel::Reader::Read(resolver.session, fieldDefIdentOpt.value());
                    auto actualFieldType = TermManager::Resolve(resolver.session, fieldDef.FieldType());
                    if (ref.fieldType != actualFieldType) {
                        log.Stream(Logging::Level::ERROR)
                            << "Field type mismatch expected:  " << ref.fieldType.GetName(resolver.session)
                            << ", actual: " << actualFieldType.GetName(resolver.session) << Stream::endl;
                        return std::nullopt;
                    }

                    uintptr_t location = StaticsManager::Of(resolver.session)
                                             .GetLocation(resolver.session, typeDefIdent, fieldDefIdentOpt.value());

                    return StaticField::Content { refType, ref.name, fieldType, location };
                }
            }
            default: {
                FATAL("Not supported yet %d", ref.refType.GetKind());
                return std::nullopt;
            }
        }
    }

    template <typename T> static std::optional<typename T::Content*> ProbeCache(Index<T> id, Resolver::Cache<T>& cache)
    {
        auto it = cache.find(id.GetValue());
        if (it != cache.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    static ResolvedMethodReference ResolveReference(
        Session& session, TermManager& manager, RefIdentifier<Symlevel::MethodReference> identifier
    )
    {
        auto parsedRef = Symlevel::MethodReference::Parse(session, identifier);
        auto refType   = manager.Resolve(session, parsedRef.refType);
        auto name      = Symlevel::String::Parse(session, parsedRef.name);
        auto signature = manager.Resolve(session, parsedRef.methodSig);
        auto flags     = parsedRef.flags;

        bool isResolved = true;
        if (refType.GetKind() == TermKind::UNDEFINED || signature.GetKind() == TermKind::UNDEFINED) {
            // undef terms would be reported separately
            log.Log(Logging::Level::ERROR, [&](Stream::Output& stream) {
                Stream::ResolvingOutput out(session, stream);
                out << "Failed to parse method reference " << identifier << Stream::endl;
            });
            isResolved = false;
        }

        return { refType, name, signature, identifier, flags, isResolved };
    }

    template <typename Call> static ResolvedMethodReference ResolveReference(Resolver& resolver, Index<Call> index)
    {
        auto ident = RefIdentifier<Symlevel::MethodReference>(index, resolver.method.GetFileId());
        auto ref   = ResolveReference(resolver.session, resolver.termManager, ident);

        log.Log(Logging::Level::INFO, [&](Stream::Output& stream) {
            Stream::ResolvingOutput out(resolver.session, stream);
            out << "Resolving method: " << ref.refType << "." << ref.name << ref.signature << Stream::endl;
        });

        return ref;
    }

    static ResolvedFieldReference ResolveReference(
        Session& session, TermManager& manager, RefIdentifier<Symlevel::FieldReference> identifier
    )
    {
        auto parsedRef = Symlevel::FieldReference::Parse(session, identifier);
        auto refType   = manager.Resolve(session, parsedRef.refType);
        auto name      = Symlevel::String::Parse(session, parsedRef.name);
        auto fieldType = manager.Resolve(session, parsedRef.fieldType);
        return { refType, name, fieldType, identifier, parsedRef.isRecord };
    }

    static MethodSignature ConstructSignature(Resolver& resolver, ResolvedMethodReference& ref)
    {
        auto signature = ref.signature;
        ASSERTION(signature.GetLength() > 0, "method signature encoding was incorrect");

        auto paramLength = signature.GetLength() - 1;
        auto retTypeIdx  = paramLength;

        std::vector<Type> params;
        params.reserve(paramLength);
        for (int i = 0; i < paramLength; i++) {
            params.push_back(Type(signature.Subterm(i), resolver));
        }
        return {
            .resolver = &resolver,
            .term     = signature,
        };
    }

    static std::optional<VirtualCall::Content> ResolveCbcCall(Resolver& resolver, ResolvedMethodReference& ref)
    {
        auto& manager = MethodTableManager::Of(resolver.session);
        auto optMT    = manager.GetMethodTable(resolver.session, ref.refType);
        auto refType  = Type(ref.refType, resolver);
        auto sret     = ref.flags.Is(Symlevel::MethodRefFlag::SRET);

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

        auto method = Symlevel::Reader::Read(resolver.session, resolved->method);
        auto actualFlags = method.GetABIFlags();
        if (actualFlags != ref.flags) {
            log.Log(Logging::Level::ERROR, [&](Stream::Output& stream) {
                stream << "Resolved method is abi incompatible " << ref.GetFullName(resolver.session) << Stream::endl;
            });
            return std::nullopt;
        }

        auto sig = ConstructSignature(resolver, ref);
        return VirtualCall::Content { refType, ref.name, std::move(sig), resolved->methodNum, resolved->subTableNum,
                                      sret };
    }

    static std::optional<InterfaceCall::Content> ResolveCall(Resolver& resolver, Index<InterfaceCall> id)
    {
        auto [file, raf] = resolver.session.File(resolver.method.GetFileId());
        auto ref         = ResolveReference(resolver, id);
        if (!ref.isResolved) {
            return std::nullopt;
        }
        auto refType = resolver.Wrap(ref.refType);
        auto sret    = ref.flags.Is(Symlevel::MethodRefFlag::SRET);

        if (ref.flags.Is(Symlevel::MethodRefFlag::AOT)) {
                auto data = file.GetInterfaceCallAotTable().GetData(resolver.session, ref.identifier.GetIndex());
                auto sig  = ConstructSignature(resolver, ref);
                return InterfaceCall::Content { refType, ref.name, std::move(sig), data.inum, sret };
        } else {
            auto call = ResolveCbcCall(resolver, ref);
            if (call.has_value()) {
                return InterfaceCall::Content {
                    call->refType, call->name, std::move(call->signature), call->methodNum, sret
                };
            }
            return std::nullopt;
        }
    }

    static std::optional<VirtualCall::Content> ResolveCall(Resolver& resolver, Index<VirtualCall> id)
    {
        auto [file, raf] = resolver.session.File(resolver.method.GetFileId());
        auto ref         = ResolveReference(resolver, id);
        if (!ref.isResolved) {
            return std::nullopt;
        }
        auto refType = resolver.Wrap(ref.refType);
        auto sret    = ref.flags.Is(Symlevel::MethodRefFlag::SRET);

        if (ref.flags.Is(Symlevel::MethodRefFlag::AOT)) {
            auto data = file.GetVirtualCallAotTable().GetData(resolver.session, ref.identifier.GetIndex());
            auto sig  = ConstructSignature(resolver, ref);
            return VirtualCall::Content { refType, ref.name, std::move(sig), data.methodNum, data.extDefNum, sret };
        } else {
            return ResolveCbcCall(resolver, ref);
        }
    }

    static std::optional<DirectCall::Content> ResolveAotDirectCall(Resolver& resolver, ResolvedMethodReference& ref)
    {
        auto [file, raf] = resolver.session.File(resolver.method.GetFileId());
        auto data        = file.GetDirectCallAotTable().GetData(resolver.session, ref.identifier.GetIndex());

        auto linkageName = Symlevel::String::Parse(resolver.session, data.linkangeName);
        auto funcPtr     = file.GetDependencies().FindTarget(linkageName);

        if (!funcPtr) {
            log.Log(Logging::Level::FATAL, [linkageName](Stream::Output& stream) {
                stream << "not found function: " << linkageName << Stream::endl;
            });
            return std::nullopt;
        }

        auto sig = ConstructSignature(resolver, ref);

        /// FIXME: choose proper adapter based on signature (abi)
        DirectCall::CallData callData = DirectCall::Compiled {
            .funcPtr    = reinterpret_cast<uintptr_t>(funcPtr),
            .i2cAdapter = RTSupport::Adapters::GenericI2CCallInstance(),
        };

        auto refType = resolver.Wrap(ref.refType);
        return DirectCall::Content { refType, ref.name, std::move(sig), callData };
    }

    static std::optional<DirectCall::Content> ResolveCall(Resolver& resolver, Index<DirectCall> id)
    {
        auto [file, raf] = resolver.session.File(resolver.method.GetFileId());
        auto ref         = ResolveReference(resolver, id);
        if (!ref.isResolved) {
            return std::nullopt;
        }

        if (ref.flags.Is(Symlevel::MethodRefFlag::AOT)) {
            return ResolveAotDirectCall(resolver, ref);
        }


        auto termIdent = TypeTermId(ref.refType);
        auto type      = Symlevel::Reader::Read(resolver.session, termIdent.GetIdentifier());

        // FIXME: search in hierarchy
        auto method = [&]() -> std::optional<Identifier<Symlevel::MethodDefinition>> {
            for (auto m : resolver.session.Decoder().FindBucket(type.GetMethods(), ref.name)) {
                auto def = Symlevel::Reader::Read(resolver.session, m);
                auto sig = TermManager::Resolve(resolver.session, def.Signature());
                if (sig == ref.signature) {
                    return m;
                }
            }

            for (auto m : type.GetVirtualMethods().Values(resolver.session)) {
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
        auto sig     = ConstructSignature(resolver, ref);
        auto refType = resolver.Wrap(ref.refType);
        // TODO: simplify
        if (auto* staticFuh = std::get_if<Interpretation::StaticFunctionHandle*>(&fuh)) {
            auto fuh                  = *staticFuh;
            DirectCall::CallData data = DirectCall::Compiled {
                .funcPtr    = reinterpret_cast<uintptr_t>(fuh->function),
                .i2cAdapter = fuh->base.i2call,
            };
            return DirectCall::Content { refType, ref.name, std::move(sig), data };
        } else {
            auto dynFuh               = std::get<Interpretation::DynamicFunctionHandle*>(fuh);
            DirectCall::CallData data = dynFuh;
            return DirectCall::Content { refType, ref.name, std::move(sig), data };
        }
    }
};

std::optional<VirtualCall> Resolver::Query(Index<VirtualCall> id)
{
    if (auto opt = ResolverProxy::ProbeCache(id, dynamicCalls); opt.has_value()) {
        return VirtualCall { opt.value() };
    }
    if (auto opt = ResolverProxy::ResolveCall(*this, id); opt.has_value()) {
        auto res = session.Allocator().New<VirtualCall::Content>(opt.value());
        dynamicCalls.insert({ id.GetValue(), res });
        return VirtualCall { res };
    }
    return std::nullopt;
}

std::optional<InterfaceCall> Resolver::Query(Index<InterfaceCall> id)
{
    if (auto opt = ResolverProxy::ProbeCache(id, interfaceCalls); opt.has_value()) {
        return InterfaceCall { opt.value() };
    }
    if (auto opt = ResolverProxy::ResolveCall(*this, id); opt.has_value()) {
        auto res = session.Allocator().New<InterfaceCall::Content>(opt.value());
        interfaceCalls.insert({ id.GetValue(), res });
        return InterfaceCall { res };
    }
    return std::nullopt;
}

std::optional<DirectCall> Resolver::Query(Index<DirectCall> id)
{
    if (auto opt = ResolverProxy::ProbeCache(id, directCalls); opt.has_value()) {
        return DirectCall { opt.value() };
    }
    if (auto opt = ResolverProxy::ResolveCall(*this, id); opt.has_value()) {
        auto res = session.Allocator().New<DirectCall::Content>(opt.value());
        directCalls.insert({ id.GetValue(), res });
        return DirectCall { res };
    }
    return std::nullopt;
}

std::optional<InstanceField> Resolver::Query(Index<InstanceField> id)
{
    if (auto opt = ResolverProxy::ProbeCache(id, instanceFields); opt.has_value()) {
        return InstanceField { opt.value() };
    }
    if (auto opt = ResolverProxy::ResolveField(*this, id); opt.has_value()) {
        auto res = session.Allocator().New<InstanceField::Content>(opt.value());
        instanceFields.insert({ id.GetValue(), res });
        return InstanceField { res };
    }
    return std::nullopt;
}

std::optional<StaticField> Resolver::Query(Index<StaticField> id)
{
    if (auto opt = ResolverProxy::ProbeCache(id, staticFields); opt.has_value()) {
        return StaticField { opt.value() };
    }
    if (auto opt = ResolverProxy::ResolveField(*this, id); opt.has_value()) {
        auto res = session.Allocator().New<StaticField::Content>(opt.value());
        staticFields.insert({ id.GetValue(), res });
        return StaticField { res };
    }
    return std::nullopt;
}

std::optional<InstanceField> Resolver::QueryTupleElement(Type refType, uint32_t idx)
{
    auto term = refType.term;
    ASSERT(term.GetKind() == TermKind::TUPLE);
    auto optTypeInfo = refType.GetTypeInfo();
    if (!optTypeInfo.has_value()) {
        return std::nullopt;
    }
    ASSERT(idx < term.GetLength());
    auto typeInfo       = *optTypeInfo;
    auto offset         = RTSupport::Execution::GetFieldOffset(typeInfo, idx, false);
    auto fieldType      = Type(term.Subterm(idx), this);
    InstanceField::Content field = {
        .refType   = refType,
        .name      = "",
        .fieldType = fieldType,
        .ordinal   = idx,
        .offset    = offset,
    };
    return InstanceField { session.Allocator().New<InstanceField::Content>(field) };
}

std::optional<Type> Resolver::QueryFutureByFunctional(Index<Type> id)
{
    auto optFunctional = Query(id);
    if (!optFunctional.has_value()) {
        return std::nullopt;
    }
    auto term = optFunctional->term;
    ASSERT(term.GetKind() == TermKind::FUNCTIONAL);
    ASSERT(term.GetLength() > 0);
    auto retType = term.Subterm(term.GetLength() - 1);

    std::vector<Term> subterms { retType };
    auto futureType = termManager.NewAotTerm(session, "std.core:Future", subterms, true);
    return Type(futureType, this);
}

Type Resolver::Wrap(Term term) { return Type(term, this); }

std::optional<Type> Resolver::Query(Index<Type> id)
{
    // terms are being cached on different level
    auto ident = RefIdentifier<Term>(id, method.GetFileId());
    auto term  = termManager.Resolve(session, ident);
    if (term.GetKind() == TermKind::UNDEFINED) {
        return std::nullopt;
    }
    return Type(term, this);
}

std::string_view Resolver::QueryString(uint32_t stringOffs)
{
    using namespace Symlevel;
    return Reader::Read(session, Identifier(Offset<String>(stringOffs), method.GetFileId()));
}

void Resolver::GetFullName(Type type, Stream::Output& stream) { type.term.GetName(session, stream); }

std::optional<RTSupport::TypeInfo> Resolver::GetTypeInfo(Type type)
{
    return tiManager.AcquireTypeInfo(session, type.term);
}

Stream::Output& operator<<(Stream::Output& stream, Type const& type)
{
    type.resolver->GetFullName(type, stream);
    return stream;
}

Stream::Output& operator<<(Stream::Output& out, MethodSignature const& sig)
{
    Stream::ResolvingOutput stream(sig.resolver->session, out);
    stream << "(";
    auto sep = " ";
    for (auto t : sig.Params()) {
        stream << sep << t;
        sep = ", ";
    }
    stream << ")";
    stream << sig.ResType();
    return out;
}

Stream::Output& operator<<(Stream::Output& stream, DirectCall const& call)
{
    stream << call->refType << '.' << call->name << call->signature;
    return stream;
}

Stream::Output& operator<<(Stream::Output& stream, VirtualCall const& call)
{
    stream << call->refType << '.' << call->name << call->signature;
    return stream;
}

Stream::Output& operator<<(Stream::Output& stream, InterfaceCall const& call)
{
    stream << call->refType << '.' << call->name << call->signature;
    return stream;
}

Stream::Output& operator<<(Stream::Output& stream, InstanceField const& field)
{
    stream << field->refType << '.' << field->name << '.' << field->fieldType;
    return stream;
}

Stream::Output& operator<<(Stream::Output& stream, StaticField const& field)
{
    stream << field->refType << '.' << field->name << '.' << field->fieldType;
    return stream;
}

Type MethodSignature::ResType() const { return Type(term.Subterm(term.GetLength() - 1), resolver); }

Term::Range MethodSignature::Params() const
{
    return Iterators::MakeRange(Term::SubTermGenerator { term.data, 0, term.GetLength() - 1 });
}

uint32_t MethodSignature::ParamCount() const { return term.GetLength() - 1; }

} // namespace Resolution
