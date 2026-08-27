#include "resolution.h"
#include "engine/decode/decoder.h"
#include "engine/engine.h"
#include "engine/field_layout.h"
#include "engine/identifiers.h"
#include "engine/image/flags.h"
#include "engine/image/reader.h"
#include "engine/method_table.h"
#include "engine/resolving_output.h"
#include "engine/statics_manager.h"
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
            auto definition     = Decode::Read(session, id.GetIdentifier());
            auto underlyingType = TermManager::Resolve(session, definition.GetEnumType());
            ClassSubstitution substitution(session, term);
            return GetKind(Wrap(substitution.Substitute(underlyingType)));
        }
        case TK::LAST: return CbcTypeKind::INVALID;

        default: FATAL("Unexpected %d", term.GetKind());
    }
}

std::optional<uint32_t> Resolver::GetFlatSize(Type type) { return fieldManager->GetFlatSize(type.term); }

Resolver::Resolver(Session& session, Identifier<Image::MethodDefinition> method)
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
    RefIdentifier<Image::MethodReference> identifier;
    Image::MethodRefFlags flags;
    bool isResolved;

    std::string GetFullName(Session& session)
    {
        Stream::StringBuffer buf;
        buf << refType.GetName(session) << '.' << name << signature.GetName(session);
        return buf.ToString();
    }
};

/// @brief Resolved Single or ConstIndex field.
struct ResolvedSimpleFieldRef {
    Term refType;
    std::variant<std::string_view, uint32_t> nameOrIdx;
    Term fieldType;
    RefIdentifier<Image::FieldReference> ident;
    bool isRecord;

    std::string GetFullName(Session& session)
    {
        Stream::StringBuffer buf;
        if (std::holds_alternative<std::string_view>(nameOrIdx)) {
            auto name = std::get<std::string_view>(nameOrIdx);
            buf << refType.GetName(session) << '.' << name << ' ' << fieldType.GetName(session);
        } else {
            auto idx = std::get<uint32_t>(nameOrIdx);
            buf << refType.GetName(session) << '[' << idx << "] " << fieldType.GetName(session);
        }
        return buf.ToString();
    }

    uint32_t GetRawIndex() { return ident.GetIndex().GetValue(); }
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
        Resolver& resolver, ResolvedSimpleFieldRef& ref
    )
    {
        auto refType     = resolver.Wrap(ref.refType);
        auto fieldType   = resolver.Wrap(ref.fieldType);
        auto [file, raf] = resolver.session.File(resolver.method.GetFileId());
        auto data        = Decode::GetAotData<Image::InstanceFieldAotData>(resolver, ref.ident);

        auto refTypeFlags                     = refType.term.Flags();
        std::optional<uint32_t> offset        = std::nullopt;
        std::optional<RTSupport::TypeInfo> ti = std::nullopt;

        if (refTypeFlags.isGeneric && refTypeFlags.isFixedSize) {
            // We can not query TypeInfo for generic type to access its field.
            // Since refType is fixed size type, we can query TI of any instantiation
            // of given type which will have the exact same field layout generic one.
            StubSubstitution sub(resolver, Term::Predefined(TermKind::I64));
            auto concrete = sub.Substitute(refType.term);
            ti            = resolver.tiManager.AcquireTypeInfo(resolver, concrete);
        } else if (!refTypeFlags.isGeneric) {
            ti = refType.GetTypeInfo();
        }

        if (ti) { // for generic instance fields
            offset = RTSupport::Execution::GetFieldOffset(*ti, data.ordinal, ref.refType.IsReference());
        }
        auto name = std::get<std::string_view>(ref.nameOrIdx);
        return InstanceField::Content { refType, fieldType, data.ordinal, offset, name };
    }

    static std::optional<StaticField::Content> ResolveAotStaticField(Resolver& resolver, ResolvedSimpleFieldRef& ref)
    {
        auto fileId      = resolver.method.GetFileId();
        auto refType     = resolver.Wrap(ref.refType);
        auto fieldType   = resolver.Wrap(ref.fieldType);
        auto [file, raf] = resolver.session.File(fileId);
        auto data        = Decode::GetAotData<Image::StaticFieldAotData>(resolver, ref.ident);
        auto linkageName = Decode::Read(resolver, data.linkangeName);
        auto location    = resolver.session.GetEngine().Dependencies().at(fileId).FindSymbol(linkageName);
        if (!location) {
            log.Log(Logging::Level::FATAL, [linkageName](Stream::Output& stream) {
                stream << "not found location of static field: " << linkageName << Stream::endl;
            });
            return std::nullopt;
        }
        auto name = std::get<std::string_view>(ref.nameOrIdx);
        return StaticField::Content { refType, fieldType, reinterpret_cast<uintptr_t>(location), name };
    }

    template <typename Field>
    static std::optional<typename Field::Content> ResolveSingleFieldRef(Resolver& resolver, ResolvedSimpleFieldRef ref)
    {
        auto fileId = resolver.method.GetFileId();

        if (ref.refType.GetKind() == TermKind::UNDEFINED || ref.fieldType.GetKind() == TermKind::UNDEFINED) {
            // undef terms would be reported separately
            log.Stream(Logging::Level::ERROR)
                << "Failed to parse field reference " << ref.GetRawIndex() << Stream::endl;
            return std::nullopt;
        }

        auto [file, raf] = resolver.session.File(fileId);
        auto refType     = resolver.Wrap(ref.refType);
        auto fieldType   = resolver.Wrap(ref.fieldType);
        auto name        = std::get<std::string_view>(ref.nameOrIdx);

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
                            auto def  = Decode::Read(resolver, *field.definition);
                            auto nameInDef = Decode::Read(resolver, def.GetName());
                            if (field.fieldType == ref.fieldType && nameInDef.compare(name) == 0) {
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
                    return InstanceField::Content { refType, fieldType, ordinal, optoffset, name };
                } else {
                    if (ref.refType.IsAotPromoted()) {
                        return ResolveAotStaticField(resolver, ref);
                    }
                    static_assert(std::is_same_v<Field, StaticField>);

                    auto typeDefIdent = TypeTermId(ref.refType).GetIdentifier();
                    auto typeDef      = Decode::Read(resolver, typeDefIdent);

                    auto fieldDefIdentOpt = Decode::Find(resolver, typeDef.GetFields(), name);
                    if (!fieldDefIdentOpt.has_value()) {
                        log.Stream(Logging::Level::ERROR)
                            << "Field definition search failed " << ref.GetRawIndex() << Stream::endl;
                        return std::nullopt;
                    }

                    auto fieldDef        = Decode::Read(resolver, fieldDefIdentOpt.value());
                    auto actualFieldType = TermManager::Resolve(resolver, fieldDef.FieldType());
                    if (ref.fieldType != actualFieldType) {
                        log.Stream(Logging::Level::ERROR)
                            << "Field type mismatch expected:  " << ref.fieldType.GetName(resolver.session)
                            << ", actual: " << actualFieldType.GetName(resolver.session) << Stream::endl;
                        return std::nullopt;
                    }

                    uintptr_t location = StaticsManager::Of(resolver.session)
                                             .GetLocation(resolver, typeDefIdent, fieldDefIdentOpt.value());

                    return StaticField::Content { refType, fieldType, location, name };
                }
            }
            default: {
                FATAL("Not supported yet %d", ref.refType.GetKind());
                return std::nullopt;
            }
        }
    }

    template <typename Field>
    static std::optional<typename Field::Content> ResolveConstIndexFieldRef(
        Resolver& resolver, ResolvedSimpleFieldRef ref
    );

    template <>
    std::optional<InstanceField::Content> ResolveConstIndexFieldRef<InstanceField>(
        Resolver& resolver, ResolvedSimpleFieldRef ref
    )
    {
        if (ref.refType.GetKind() == TermKind::UNDEFINED || ref.fieldType.GetKind() == TermKind::UNDEFINED) {
            // undef terms would be reported separately
            log.Stream(Logging::Level::ERROR)
                << "Failed to parse field reference " << ref.GetRawIndex() << Stream::endl;
            return std::nullopt;
        }

        auto idx     = std::get<uint32_t>(ref.nameOrIdx);
        auto refType = resolver.Wrap(ref.refType);

        TermKind kind = ref.refType.GetKind();
        switch (kind) {
            case TermKind::TUPLE: {
                return ResolveTupleElement(resolver, refType, idx);
            }
            default: {
                // TODO: support for arrays
                log.Stream(Logging::Level::ERROR)
                    << "Invalid kind in const index reference: " << static_cast<uint8_t>(kind) << Stream::endl;
                return std::nullopt;
            }
        }
    }

    template <>
    std::optional<StaticField::Content> ResolveConstIndexFieldRef<StaticField>(
        Resolver& resolver, ResolvedSimpleFieldRef ref
    )
    {
        log.Stream(Logging::Level::ERROR)
            << "ConstIndex reference cannot be resolved as static " << ref.GetRawIndex() << Stream::endl;
        return std::nullopt;
    }

    template <typename Field>
    static std::optional<typename Field::Content> ResolveMultiFieldRef(Resolver& resolver, Image::FieldReference ref);

    template <>
    std::optional<StaticField::Content> ResolveMultiFieldRef<StaticField>(Resolver& resolver, Image::FieldReference ref)
    {
        ASSERT(ref.multi.length >= 1);

        std::vector<std::variant<StaticField::Content, InstanceField::Content>> fields;
        fields.reserve(ref.multi.length);

        for (uint32_t i = 0; i < ref.multi.length; i++) {
            if (i == 0) {
                auto id    = Index<StaticField>(ref.multi.indices[i].GetValue());
                auto field = ResolveField<StaticField>(resolver, id).value();
                fields.push_back(field);
            } else {
                auto id    = Index<InstanceField>(ref.multi.indices[i].GetValue());
                auto field = ResolveField<InstanceField>(resolver, id).value();
                fields.push_back(field);
            }
        }

        auto staticField = std::get<StaticField::Content>(fields.front());

        auto refType = staticField.refType;
        auto fieldType =
            fields.size() > 1 ? std::get<InstanceField::Content>(fields.back()).fieldType : staticField.fieldType;

        std::optional<uintptr_t> location = staticField.location;

        decltype(fields) instanceFields(fields.begin() + 1, fields.end());
        for (const auto& f : instanceFields) {
            auto field = std::get<InstanceField::Content>(f);
            if (!field.offset.has_value()) {
                location = std::nullopt;
                break;
            }
            location = location.value() + field.offset.value();
        }

        return StaticField::Content { refType, fieldType, location, "<multi>" };
    }

    template <>
    std::optional<InstanceField::Content> ResolveMultiFieldRef<InstanceField>(
        Resolver& resolver, Image::FieldReference ref
    )
    {
        ASSERT(ref.multi.length >= 1);

        std::vector<InstanceField::Content> fields;
        fields.reserve(ref.multi.length);

        for (uint32_t i = 0; i < ref.multi.length; i++) {
            auto id    = Index<InstanceField>(ref.multi.indices[i].GetValue());
            auto field = ResolveField<InstanceField>(resolver, id);
            fields.push_back(field.value());
        }

        auto refType   = fields.front().refType;
        auto fieldType = fields.back().fieldType;

        std::optional<uint32_t> optOffset = 0;
        for (const auto& f : fields) {
            if (!f.offset.has_value()) {
                optOffset = std::nullopt;
                break;
            }
            optOffset = optOffset.value() + f.offset.value();
        }

        return InstanceField::Content { refType, fieldType, std::nullopt, optOffset, "<multi>" };
    }

    template <typename Field>
    static std::optional<typename Field::Content> ResolveNoneFieldRef(Resolver& resolver, Image::FieldReference ref)
    {
        auto resolvedSig = resolver.termManager.Resolve(resolver.session, ref.none.sig);
        auto sig         = resolver.Wrap(resolvedSig);
        if constexpr (std::is_same_v<Field, InstanceField>) {
            return InstanceField::Content { sig, sig, std::nullopt, std::nullopt, "<none>" };
        } else {
            return StaticField::Content { sig, sig, std::nullopt, "<none>" };
        }
    }

    template <typename Field>
    static std::optional<typename Field::Content> ResolveField(Resolver& resolver, Index<Field> id)
    {
        auto refId = Image::RefId<Image::FieldReference>(id.GetValue());
        auto ident = RefIdentifier<Image::FieldReference>(refId, resolver.method.GetFileId());

        Image::FieldReference parsedRef = Image::Reader::Read(resolver.session, ident);
        switch (parsedRef.tag) {
            case Image::SINGLE: {
                auto ref = SingleReference(resolver, parsedRef, ident);
                return ResolveSingleFieldRef<Field>(resolver, ref);
            }
            case Image::CONST_INDEX: {
                auto ref = ConstIndexReference(resolver, parsedRef, ident);
                return ResolveConstIndexFieldRef<Field>(resolver, ref);
            }
            case Image::MULTI: {
                return ResolveMultiFieldRef<Field>(resolver, parsedRef);
            }
            case Image::NONE: {
                return ResolveNoneFieldRef<Field>(resolver, parsedRef);
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
        Session& session, TermManager& manager, RefIdentifier<Image::MethodReference> identifier
    )
    {
        auto parsedRef = Decode::Read(session, identifier);
        auto refType   = manager.Resolve(session, parsedRef.refType);
        auto name      = Decode::Read(session, parsedRef.name);
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
        auto ident = RefIdentifier<Image::MethodReference>(index, resolver.method.GetFileId());
        auto ref   = ResolveReference(resolver, resolver.termManager, ident);

        log.Log(Logging::Level::INFO, [&](Stream::Output& stream) {
            Stream::ResolvingOutput out(resolver, stream);
            out << "Resolving method: " << ref.refType << "." << ref.name << ref.signature << Stream::endl;
        });

        return ref;
    }

    static ResolvedSimpleFieldRef SingleReference(
        Resolver& resolver, Image::FieldReference fr, RefIdentifier<Image::FieldReference> ident
    )
    {
        auto refType   = resolver.termManager.Resolve(resolver.session, fr.single.refType);
        auto name      = Image::Reader::Read(resolver.session, fr.single.name);
        auto fieldType = resolver.termManager.Resolve(resolver.session, fr.single.fieldType);
        return { refType, name, fieldType, ident };
    }

    static ResolvedSimpleFieldRef ConstIndexReference(
        Resolver& resolver, Image::FieldReference fr, RefIdentifier<Image::FieldReference> ident
    )
    {
        auto refType   = resolver.termManager.Resolve(resolver.session, fr.constIndex.refType);
        auto idx       = fr.constIndex.idx;
        auto fieldType = resolver.termManager.Resolve(resolver.session, fr.constIndex.fieldType);
        return { refType, idx, fieldType, ident };
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
        auto optMT    = manager.GetMethodTable(resolver, ref.refType);
        auto refType  = Type(ref.refType, resolver);
        auto sret     = ref.flags.Is(Image::MethodRefFlag::SRET);

        if (!optMT.has_value()) {
            return std::nullopt;
        }
        auto mt = *optMT;

        MethodTable::Reference mtRef = {
            .name      = ref.name,
            .signature = ref.signature,
        };
        auto resolved = mt->Resolve(resolver, mtRef);

        if (!resolved.has_value()) {
            log.Log(Logging::Level::ERROR, [&](Stream::Output& stream) {
                stream << "Failed to resolve method " << ref.GetFullName(resolver.session) << Stream::endl;
            });
            return std::nullopt;
        }

        auto method      = Decode::Read(resolver, resolved->method);
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
        auto sret    = ref.flags.Is(Image::MethodRefFlag::SRET);

        if (ref.flags.Is(Image::MethodRefFlag::AOT)) {
            auto data = Decode::GetAotData<Image::InterfaceCallAotData>(resolver, ref.identifier);
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
        auto sret    = ref.flags.Is(Image::MethodRefFlag::SRET);

        if (ref.flags.Is(Image::MethodRefFlag::AOT)) {
            auto data = Decode::GetAotData<Image::VirtualCallAotData>(resolver, ref.identifier);
            auto sig  = ConstructSignature(resolver, ref);
            return VirtualCall::Content { refType, ref.name, std::move(sig), data.methodNum, data.extDefNum, sret };
        } else {
            return ResolveCbcCall(resolver, ref);
        }
    }

    static std::optional<DirectCall::Content> ResolveAotDirectCall(Resolver& resolver, ResolvedMethodReference& ref)
    {
        auto fileId      = resolver.method.GetFileId();
        auto [file, raf] = resolver.session.File(resolver.method.GetFileId());
        auto data        = Decode::GetAotData<Image::DirectCallAotData>(resolver, ref.identifier);

        auto linkageName = Decode::Read(resolver, data.linkangeName);
        auto funcPtr     = resolver.session.GetEngine().Dependencies().at(fileId).FindSymbol(linkageName);

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

        if (ref.flags.Is(Image::MethodRefFlag::AOT)) {
            return ResolveAotDirectCall(resolver, ref);
        }

        auto termIdent = TypeTermId(ref.refType);
        auto type      = Decode::Read(resolver, termIdent.GetIdentifier());

        // FIXME: search in hierarchy
        auto method = [&]() -> std::optional<Identifier<Image::MethodDefinition>> {
            for (auto m : Decode::FindBucket(resolver, type.GetMethods(), ref.name)) {
                auto def = Decode::Read(resolver, m);
                auto sig = TermManager::Resolve(resolver, def.Signature());
                if (sig == ref.signature) {
                    return m;
                }
            }

            for (auto m : Decode::Resolve(resolver, type.GetVirtualMethods())) {
                auto def = Decode::Read(resolver, m);
                auto sig = TermManager::Resolve(resolver, def.Signature());
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

        auto fuh = Interpretation::FunctionHandleManager::Of(resolver.session).AcquireTagged(resolver, method.value());
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

    static std::optional<InstanceField::Content> ResolveTupleElement(Resolver& resolver, Type refType, uint32_t idx)
    {
        auto term = refType.term;
        ASSERT(term.GetKind() == TermKind::TUPLE);
        auto optTypeInfo = refType.GetTypeInfo();
        if (!optTypeInfo.has_value()) {
            return std::nullopt;
        }
        ASSERT(idx < term.GetLength());
        auto typeInfo  = *optTypeInfo;
        auto offset    = RTSupport::Execution::GetFieldOffset(typeInfo, idx, false);
        auto fieldType = Type(term.Subterm(idx), resolver);
        return InstanceField::Content {
            .refType   = refType,
            .fieldType = fieldType,
            .ordinal   = idx,
            .offset    = offset,
            .name      = "<tuple>",
        };
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
    if (auto opt = ResolverProxy::ResolveTupleElement(*this, refType, idx); opt.has_value()) {
        auto res = session.Allocator().New<InstanceField::Content>(opt.value());
        return InstanceField { res };
    }
    return std::nullopt;
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
    using namespace Image;
    return Reader::Read(session, Image::Identifier(Offset<String>(stringOffs), method.GetFileId()));
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
