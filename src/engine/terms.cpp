#include "engine/terms.h"
#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/resolving_output.h"
#include "engine/symlevel/cbc_file.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/io/file_id.h"
#include "engine/symlevel/io/stream_file_reader.h"
#include "engine/symlevel/reader.h"
#include "engine/symlevel/region_data.h"
#include "engine/symlevel/type_kind.h"
#include "string.h"
#include "utils/assertion.h"
#include "utils/heap.h"
#include "utils/iterators.h"
#include "utils/ostream.h"
#include <alloca.h>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_set>

namespace Engine {

/// Internal representation of `Term`.
/// The main things which are needed to represent term is an identifier and subterms.
/// The length of subterm array is bounded by 2^16, so in the leftover memory
/// we fit additional fields `hash` and `isLocal`.
struct TermData {
    TermId identifier;
    uint32_t hash;
    uint16_t length;
    TermFlags flags;
    Term subterms[];

    void InitAfterSubterms(TermId identifier, uint16_t length, TermFlags flags)
    {
        uint32_t hash = 0;
        for (int i = 0; i < length; i++) {
            hash = 31 * hash + subterms->Hash();
        }
        Init(identifier, identifier.Hash() ^ hash, length, flags);
    }

    void Init(TermId identifier, uint32_t hash, uint16_t length, TermFlags flags)
    {
        this->identifier = identifier;
        this->length     = length;
        this->hash       = hash;
        this->flags      = flags;
    }
};

enum Tag : uint8_t {
    NIL               = 0x0,
    REF               = 0x1,
    AOT_REF           = 0x2,
    CANGJIE_ARRAY     = 0x3,
    VARRAY            = 0x4,
    ENUM_WRAPPER      = 0x5,
    C_POINTER         = 0x6,
    FUNC_TYPE_VAR     = 0x7,
    CLASS_TYPE_VAR    = 0x8,
    GENERIC_RECORD    = 0x9,
    GENERIC_REFERENCE = 0xa,
    NULLABLE          = 0xb,
    FUNCTIONAL        = 0xc,
    REC               = 0xd,
    AOT_REC           = 0xe,
    NON_NULLABLE      = 0xf,
    GENERIC_AOT_REF   = 0x10,
    GENERIC_AOT_REC   = 0x11,
    TUPLE             = 0x12,
    BOX               = 0x13,
    FST               = 0x14,
    GENERIC_OPTION    = 0x15,
    NULLABLE_OPTION   = 0x16,
    UNION_OPTION      = 0x17,
    UNION_ENUM        = 0x18,
    PRIMITIVE_ENUM    = 0x19,
};

static TermData* AllocateTerm(Memory::Heap& allocator, size_t subtermCount = 0)
{
    return static_cast<TermData*>(allocator.Allocate(sizeof(TermData) + subtermCount * sizeof(Term), alignof(TermData))
    );
}

struct BuiltinTerms {
    void* memory;
    void* primitives;
    void* classTypeVars;
    void* funcTypeVars;

    static constexpr size_t TV_COUNT   = 256;
    static constexpr size_t PRIM_COUNT = FIRST_NON_PRIMITIVE;

    BuiltinTerms(BuiltinTerms const&) = delete;

    ~BuiltinTerms() { std::free(memory); }

    inline static TermData* DataAt(void* memory, size_t idx)
    {
        char* ptr  = reinterpret_cast<char*>(memory);
        ptr       += sizeof(TermData) * idx;
        return reinterpret_cast<TermData*>(ptr);
    }

    inline TermData* Primitive(size_t i) const
    {
        ASSERT(i < PRIM_COUNT);
        return DataAt(primitives, i);
    }

    inline TermData* ClassTv(size_t i) const
    {
        ASSERT(i < TV_COUNT);
        return DataAt(classTypeVars, i);
    }

    inline TermData* FuncTv(size_t i) const
    {
        ASSERT(i < TV_COUNT);
        return DataAt(funcTypeVars, i);
    }

    BuiltinTerms()
    {
        size_t seed = 0xf123123a;

        auto hash = [&seed]() {
            constexpr size_t multiplier = 3202034522624059733L;
            constexpr size_t addend     = 0x421L;
            auto next                   = (multiplier * seed + addend);
            return seed                 = next;
        };

        void* memory = std::malloc(sizeof(TermData) * (TV_COUNT + TV_COUNT + PRIM_COUNT));
        if (!memory)
            FATAL("Failed to allocate builtin terms");

        void* primitives    = DataAt(memory, 0);
        void* classTypeVars = DataAt(memory, PRIM_COUNT);
        void* funcTypeVars  = DataAt(memory, PRIM_COUNT + TV_COUNT);

        TermFlags primFlags = {0};

        TermFlags tvFlags = {
            .isReference   = true,
            .isGeneric     = true,
        };

        for (size_t i = 0; i < PRIM_COUNT; i++) {
            auto kind        = TermKind(i);
            auto data        = DataAt(primitives, i);
            data->hash       = hash();
            data->length     = 0;
            data->identifier = TagTermId(kind);
            data->flags      = primFlags;
        }

        for (size_t i = 0; i < TV_COUNT; i++) {
            auto data        = DataAt(classTypeVars, i);
            data->hash       = hash();
            data->length     = 0;
            data->identifier = ClassTvTermId(i);
            data->flags      = tvFlags;
        }

        for (size_t i = 0; i < TV_COUNT; i++) {
            auto data        = DataAt(funcTypeVars, i);
            data->hash       = hash();
            data->length     = 0;
            data->identifier = FuncTvTermId(i);
            data->flags      = tvFlags;
        }

        this->memory = memory;
        this->primitives = primitives;
        this->classTypeVars = classTypeVars;
        this->funcTypeVars = funcTypeVars;
    }
};

static BuiltinTerms g_Builtins;

GlobalTerm Term::Predefined(TermKind tk)
{
    int num = static_cast<int>(tk);
    return GlobalTerm(g_Builtins.Primitive(num));
}

Term Term::ClassTypeVariable(uint8_t tv) { return GlobalTerm(g_Builtins.ClassTv(tv)); }

Term Term::FuncTypeVariable(uint8_t tv) { return GlobalTerm(g_Builtins.FuncTv(tv)); }

Term Term::Definition(Session& session, Identifier<Symlevel::TypeDefinition> type)
{
    // TODO: handle arity and generic type vars
    auto def   = Symlevel::Reader::Read(session, type);
    bool isRec = def.GetFlags().Is(Symlevel::TypeKind::RECORD);
    auto arity = def->arity;

    auto* data = AllocateTerm(session.Allocator(), arity);
    for (uint8_t i = 0; i < arity; i++) {
        data->subterms[i] = ClassTypeVariable(i);
    }
    data->InitAfterSubterms(
        TypeTermId(type),
        arity,
        {
            .isLocal       = true,
            .isReference   = !isRec,
            .isAotPromoted = false,
            .isGeneric     = (arity > 0),
        }
    );
    return LocalTerm(data);
}

static Term Undefined(Session& session, RefIdentifier<Term> termId)
{
    // TODO: assertions for length
    auto* data = AllocateTerm(session.Allocator());
    data->InitAfterSubterms(
        UndefTermId(termId),
        0,
        {
            .isLocal       = true,
            .isReference   = !false,
            .isAotPromoted = false,
            .isGeneric     = false,
        }
    );
    return LocalTerm(data);
}

static bool CompareTermData(TermData* origin, TermData* another)
{
    if (another == origin) {
        return true;
    } else if (another->hash != origin->hash) {
        return false;
    } else if (another->identifier != origin->identifier) {
        return false;
    } else if (another->length != origin->length) {
        return false;
    } else {
        auto length = origin->length;
        for (auto i = 0; i < length; i++) {
            if (!CompareTermData(another->subterms[i].data, origin->subterms[i].data)) {
                return false;
            }
        }
        return true;
    }
}

Term::Term() : Term(Term::Predefined(TermKind::NIL)) {}

Term::Term(TermData* data) : data(data) {}

LocalTerm Term::AsLocal()
{
    ASSERT(IsLocal());
    return LocalTerm(data);
}

GlobalTerm Term::AsGlobal()
{
    ASSERT(!IsLocal());
    return GlobalTerm(data);
}

Term Term::Subterm(uint32_t i) const { return data->subterms[i]; }

TermId Term::GetId() const { return data->identifier; }

TermKind Term::GetKind() const { return data->identifier.GetKind(); }

bool Term::IsFloat() const
{
    switch (GetKind()) {
        case TermKind::F32:
        case TermKind::F64: return true;
        default:            return false;
    }
}

uint32_t Term::GetLength() const { return data->length; }

uint32_t Term::Hash() const { return data->hash; }

std::string Term::GetName(Session& session) const
{
    Stream::StringBuffer buf;
    GetName(session, buf);
    return buf.ToString();
}

void Term::GetName(Session& session, Stream::Output& out) const
{
    Stream::ResolvingOutput stream(session, out);
    auto printSubTerms = [&](std::string_view prefix, std::string_view suffix, int len) {
        stream << prefix;
        auto separator = "";
        for (int i = 0; i < len; i++) {
            stream << separator << Subterm(i);
            separator = ", ";
        }
        stream << suffix;
    };

    using TK  = TermKind;
    auto kind = GetKind();
    switch (kind) {
        case TK::NIL:     stream << "Nil"; break;
        case TK::VOID:    stream << "Void"; break;
        case TK::UNIT:    stream << "Unit"; break;
        case TK::NOTHING: stream << "Nothing"; break;
        case TK::BOOLEAN: stream << "Bool"; break;
        case TK::I8:      stream << "Int8"; break;
        case TK::U8:      stream << "UInt8"; break;
        case TK::I16:     stream << "Int16"; break;
        case TK::U16:     stream << "UInt16"; break;
        case TK::I32:     stream << "Int32"; break;
        case TK::U32:     stream << "UInt32"; break;
        case TK::UCHAR32: stream << "UChar32"; break;
        case TK::I64:     stream << "Int64"; break;
        case TK::U64:     stream << "UInt64"; break;
        case TK::IADDR:   stream << "IAddr"; break;
        case TK::UADDR:   stream << "UAddr"; break;
        case TK::BSTRING: stream << "BString"; break;
        case TK::F16:     stream << "Float16"; break;
        case TK::F32:     stream << "Float32"; break;
        case TK::F64:     stream << "Float64"; break;

        case TK::UNDEFINED: {
            auto undef  = UndefTermId(*this).GetIdentifier();
            auto file   = undef.GetFileId();
            auto region = undef.GetIndex().GetRegion();
            auto index  = undef.GetIndex().GetIndex();
            out.PrintFmt("$unresolved<%u,%u,%u>", file.id, region, index);
            break;
        }

        case TK::BOX: {
            stream << "$box<" << Subterm(0) << '>';
            break;
        }

        case TK::C_POINTER: {
            stream << "CPointer<" << Subterm(0) << '>';
            break;
        }

        case TK::NULLABLE: {
            stream << "$nullable<" << Subterm(0) << '>';
            break;
        }

        case TK::CANGJIE_ARRAY: {
            stream << "$array<" << Subterm(0) << '>';
            break;
        }

        case TK::FUNCTIONAL: {
            printSubTerms("(", ") -> ", GetLength() - 1);
            stream << Subterm(GetLength() - 1);
            break;
        }

        case TK::TUPLE: {
            printSubTerms("(", ")", GetLength());
            break;
        }

        case TK::TYPE: {
            auto ident = TypeTermId(*this).GetIdentifier();
            auto type  = Symlevel::TypeDefinition::Resolve(session, ident);
            stream << Symlevel::Reader::Read(session, type.GetName());
            if (int len = GetLength(); len > 0) {
                printSubTerms("<", ">", len);
            }
            break;
        }

        case TK::AOT_TYPE: {
            auto& manager = TermManager::Of(session);
            std::string_view name = manager.GetNameOfAotType(AotTermId(*this));
            stream << name;
            if (int len = GetLength(); len > 0) {
                printSubTerms("<", ">", len);
            }
            break;
        }

        case TK::CLASS_TYPE_VAR: {
            auto tv = ClassTvTermId(*this).GetNum();
            stream << "%" << tv;
            break;
        }

        case TK::FUNC_TYPE_VAR: {
            auto tv = FuncTvTermId(*this).GetNum();
            stream << "%%" << tv;
            break;
        }

        default: {
            FATAL("Unexpected case %d", GetKind());
        }
    }
}

bool Term::operator!=(const Term& another) const { return !(*this == another); }

bool Term::operator==(const Term& another) const { return CompareTermData(this->data, another.data); }

bool Term::IsLocal() const { return data->flags.isLocal; }

LocalTerm::LocalTerm(TermData* data) : Term(data) { ASSERT(data->flags.isLocal); }

GlobalTerm::GlobalTerm(TermData* data) : Term(data) { ASSERT(!data->flags.isLocal); }

GlobalTerm LocalTerm::Publish(Session& session)
{
    Term term(*this);
    return TermManager::Of(session).Globalize(term);
}

bool GlobalTerm::operator==(const GlobalTerm& another) const { return data == another.data; }

bool GlobalTerm::operator!=(const GlobalTerm& another) const { return data != another.data; }

GlobalTerm TermManager::Globalize(Term& term)
{
    if (!term.IsLocal()) {
        return term.AsGlobal();
    }

    // globalize terms in-place
    auto termData = term.data;
    for (int i = 0; i < term.GetLength(); i++) {
        termData->subterms[i] = Globalize(termData->subterms[i]);
    }

    std::lock_guard guard(lock);

    // query cache without allocating a new term
    auto it = cache.find(termData);
    if (it != cache.end()) {
        return GlobalTerm(*it);
    }

    // cache miss; evacuate term and update cache
    auto data = static_cast<TermData*>(malloc(sizeof(TermData) + termData->length * sizeof(Term)));
    if (data == nullptr) {
        FATAL("Out of memory");
    }

    for (int i = 0; i < term.GetLength(); i++) {
        data->subterms[i] = termData->subterms[i];
    }
    auto flags    = termData->flags;
    flags.isLocal = false;
    data->Init(termData->identifier, termData->hash, termData->length, flags);

    cache.insert(data);
    term.data = data;
    return term.AsGlobal();
}

static bool IsProperTypeReference(Symlevel::TypeDefinition& def, bool isReference, int arity)
{
    if ((def.GetFlags().Is(Symlevel::TypeKind::RECORD)) == isReference) {
        return false;
    } else if (def->arity != arity) {
        return false;
    }
    return true;
}

Term TermManager::NewAotTerm(
    Session& session, std::string_view name, std::vector<Term> const& subterms, bool isReference
)
{
    auto& heap     = session.Allocator();
    auto data      = AllocateTerm(heap, subterms.size());
    bool isGeneric = false;
    auto arity     = subterms.size();
    for (int i = 0; i < arity; i++) {
        data->subterms[i] = subterms[i];
        isGeneric         = isGeneric || subterms[i].IsGeneric();
    }

    TermId id       = TagTermId(TermKind::NOTHING);
    TermFlags flags = {
        .isLocal       = true,
        .isReference   = isReference,
        .isGeneric     = isGeneric,
    };

    auto type = session.GetEngine().FindType(session, name);
    if (type.has_value()) {
        ASSERT([&]() -> bool {
            auto def = Symlevel::TypeDefinition::Resolve(session, type.value());
            return IsProperTypeReference(def, isReference, arity);
        }());
        id                  = TypeTermId(*type);
        flags.isAotPromoted = true;
    } else {
        id = AotTermId(InternString(name));
    }
    data->InitAfterSubterms(id, arity, flags);
    return Term(LocalTerm(data));
}

Term TermManager::NewTermWithId(Session& session, TermId id, bool isReference, std::vector<Term> const& subterms)
{
    auto& heap     = session.Allocator();
    auto data      = AllocateTerm(heap, subterms.size());
    bool isGeneric = false;
    auto arity     = subterms.size();
    for (int i = 0; i < arity; i++) {
        data->subterms[i] = subterms[i];
        isGeneric         = isGeneric || subterms[i].IsGeneric();
    }

    TermFlags flags = {
        .isLocal       = true,
        .isReference   = isReference,
        .isGeneric     = isGeneric,
    };
    data->InitAfterSubterms(id, arity, flags);
    return Term(LocalTerm(data));
}

uint64_t TermManager::Hasher::operator()(TermData* const& data) const { return data->hash; }

bool TermManager::Comparator::operator()(TermData* const& left, TermData* const& right) const
{
    if (left == right) {
        return true;
    } else if (left->hash != right->hash) {
        return false;
    } else if (left->length != right->length) {
        return false;
    } else {
        auto len = left->length;
        // shallow comparison for cache.
        for (int i = 0; i < len; i++) {
            auto lhs = left->subterms[i];
            auto rhs = right->subterms[i];
            ASSERT(!lhs.IsLocal());
            ASSERT(!rhs.IsLocal());
            if (lhs.data != rhs.data) {
                return false;
            }
        }
        return true;
    }
}

struct TermResolver {
    Symlevel::RegionData const& regionData;
    Session& session;
    Memory::Heap& heap;
    IO::FileId fileId;
    uint8_t region;
    IO::RandomAccessFile& raf;
    Symlevel::CbcFile& file;
    TermManager& manager;

    Term NewUndefined(Symlevel::RefId<Term> refId) { return Undefined(session, RefIdentifier(refId, fileId)); }

    bool ReadSubTerms(TermData* data, bool* isGenericLoc, int length, IO::StreamFileReader& reader)
    {
        using namespace Symlevel;
        bool isGeneric = false;

        for (int i = 0; i < length; i++) {
            auto subtermIdx = reader.ReadULEB();
            auto subterm    = Resolve(RefId<Term>(region, subtermIdx));
            if (subterm.GetKind() == TermKind::UNDEFINED) {
                return false;
            }
            data->subterms[i] = subterm;
            isGeneric         = isGeneric || subterm.IsGeneric();
        }
        *isGenericLoc = isGeneric;
        return true;
    }

    Term ResolveTypeDefTerm(
        IO::StreamFileReader& reader,
        Symlevel::Offset<Symlevel::String> nameOffs,
        int expectedLength,
        bool isReference,
        Symlevel::RefId<Term> refId,
        bool wasAot
    )
    {
        using namespace Symlevel;

        auto name = Reader::Read(session, fileId, nameOffs);
        return ResolveTypeDefTerm(reader, name, expectedLength, isReference, refId, wasAot);
    }

    Term ResolveTypeDefTerm(
        IO::StreamFileReader& reader,
        Symlevel::String name,
        int expectedLength,
        bool isReference,
        Symlevel::RefId<Term> refId,
        bool wasAot
    )
    {
        using namespace Symlevel;

        auto type = session.GetEngine().FindType(session, name);
        if (!type.has_value()) {
            return NewUndefined(refId);
        }
        auto identifier = type.value();

        auto def       = Symlevel::TypeDefinition::Resolve(session, identifier);
        bool undefined = !IsProperTypeReference(def, isReference, expectedLength);

        if (undefined && wasAot) {
            FATAL("Unexpected mismatch of resolved type definition and aot term");
        }
        if (undefined) {
            return NewUndefined(refId);
        }

        auto data      = AllocateTerm(heap, expectedLength);
        bool isGeneric = false;
        if (!ReadSubTerms(data, &isGeneric, expectedLength, reader)) {
            return NewUndefined(refId);
        }

        data->InitAfterSubterms(
            TypeTermId(identifier),
            expectedLength,
            {
                .isLocal       = true,
                .isReference   = isReference,
                .isAotPromoted = wasAot,
                .isGeneric     = isGeneric,
            }
        );
        return Term(LocalTerm(data));
    }

    Term ResolveOptionTerm(
        IO::StreamFileReader& reader,
        Symlevel::String name,
        int expectedLength,
        Symlevel::RefId<Term> refId,
        Tag tag
    )
    {
        using namespace Symlevel;

        auto type = session.GetEngine().FindType(session, name);
        if (!type.has_value()) {
            return NewUndefined(refId);
        }
        auto identifier = type.value();

        auto def = Symlevel::TypeDefinition::Resolve(session, identifier);

        bool optionLikeEnum = false;
        switch (def->enumKind) {
            case Symlevel::EnumKind::OPTION0:
            case Symlevel::EnumKind::OPTION1:
                optionLikeEnum = true;
            default: {}
        }

        bool undefined   = false;
        bool isReference = false;
        TermId id = TagTermId(TermKind::NIL);
        switch (tag) {
            case GENERIC_OPTION:
                isReference = true;
                undefined = !optionLikeEnum;
                id = GenericOptionId(identifier);
                break;
            case NULLABLE_OPTION:
                isReference = true;
                undefined = !optionLikeEnum;
                id = NullableOptionId(identifier);
                break;
            case UNION_OPTION:
                isReference = false;
                undefined = !optionLikeEnum;
                id = UnionOptionId(identifier);
                break;
            case UNION_ENUM:
                undefined = def->enumKind != Symlevel::EnumKind::UNION;
                id = UnionEnumId(identifier);
                break;
            case PRIMITIVE_ENUM:
                undefined = def->enumKind != Symlevel::EnumKind::PRIMITIVE;
                id = PrimitiveEnumId(identifier);
                break;
            default: {}
        }

        if (undefined || def->arity != expectedLength) {
            return NewUndefined(refId);
        }

        auto data      = AllocateTerm(heap, expectedLength);
        bool isGeneric = false;
        if (!ReadSubTerms(data, &isGeneric, expectedLength, reader)) {
            return NewUndefined(refId);
        }

        data->InitAfterSubterms(
            id,
            expectedLength,
            {
                .isLocal       = true,
                .isReference   = isReference,
                .isGeneric     = isGeneric,
            }
        );
        return Term(LocalTerm(data));
    }

    Term ResolveAotType(
        IO::StreamFileReader& reader,
        Symlevel::Offset<Symlevel::String> nameOffs,
        int length,
        bool isReference,
        Symlevel::RefId<Term> refId
    )
    {
        auto name = Symlevel::Reader::Read(session, fileId, nameOffs);
        // Attempt to find type definition, even if the type is tagged as aot.
        // Because the type could present in `TypeDefinition` super closure
        // or be present as "patch".
        auto term = ResolveTypeDefTerm(reader, name, length, isReference, refId, true);
        if (term.GetKind() != TermKind::UNDEFINED) {
            return term;
        }

        auto data      = AllocateTerm(heap, length);
        bool isGeneric = false;
        if (!ReadSubTerms(data, &isGeneric, length, reader)) {
            return NewUndefined(refId);
        }
        TermFlags flags = {
            .isLocal       = true,
            .isReference   = isReference,
            .isGeneric     = isGeneric,
        };
        auto internedName = manager.InternString(name);

        data->InitAfterSubterms(AotTermId(internedName), length, flags);
        return Term(LocalTerm(data));
    }

    Term Resolve(Symlevel::RefId<Term> refId)
    {
        using namespace Symlevel;

        if (refId.GetIndex() < FIRST_NON_PRIMITIVE) {
            return Term::Predefined(TermKind(refId.GetIndex()));
        }
        auto offset = regionData.Query(session, refId);
        IO::StreamFileReader reader(raf, file.GetTermSectionOffs() + offset);

        auto tag = static_cast<Tag>(reader.ReadU8());
        switch (tag) {
            case REC: // fall-through
            case REF: {
                auto nameOffs = Offset<String>(reader.ReadULEB());
                return ResolveTypeDefTerm(reader, nameOffs, 0, tag == REF, refId, false);
            }
            case GENERIC_RECORD:
            case GENERIC_REFERENCE: {
                auto nameOffs = Offset<String>(reader.ReadULEB());
                auto length   = reader.ReadU8();
                return ResolveTypeDefTerm(reader, nameOffs, length, tag == GENERIC_REFERENCE, refId, false);
            }
            case AOT_REC: // fall-through
            case AOT_REF: {
                auto nameOffs = Offset<String>(reader.ReadULEB());
                return ResolveAotType(reader, nameOffs, 0, tag == AOT_REF, refId);
            }
            case GENERIC_AOT_REC:
            case GENERIC_AOT_REF: {
                auto nameOffs = Offset<String>(reader.ReadULEB());
                auto length   = reader.ReadU8();
                bool isRef    = tag == GENERIC_AOT_REF;
                return ResolveAotType(reader, nameOffs, length, isRef, refId);
            }
            case FUNCTIONAL: {
                auto len   = reader.ReadU8() + 1; // +1 for ret type
                auto* data = AllocateTerm(heap, len);

                bool isGeneric = false;
                if (!ReadSubTerms(data, &isGeneric, len, reader)) {
                    return NewUndefined(refId);
                }
                TermFlags flags = {
                    .isLocal       = true,
                    .isReference   = true,
                    .isGeneric     = isGeneric,
                };
                data->InitAfterSubterms(TagTermId(TermKind::FUNCTIONAL), len, flags);
                return Term(LocalTerm(data));
            }
            case TUPLE: {
                auto len   = reader.ReadULEB();
                auto* data = AllocateTerm(heap, len);

                bool isGeneric = false;
                if (!ReadSubTerms(data, &isGeneric, len, reader)) {
                    return NewUndefined(refId);
                }
                TermFlags flags = {
                    .isLocal       = true,
                    .isGeneric     = isGeneric,
                };
                data->InitAfterSubterms(TagTermId(TermKind::TUPLE), len, flags);
                return Term(LocalTerm(data));
            }
            case NULLABLE: {
                auto* data     = AllocateTerm(heap, 1);
                bool isGeneric = false;
                if (!ReadSubTerms(data, &isGeneric, 1, reader)) {
                    return NewUndefined(refId);
                }
                TermFlags flags = {
                    .isLocal       = true,
                    .isReference   = true,
                    .isGeneric     = isGeneric,
                };
                data->InitAfterSubterms(TagTermId(TermKind::NULLABLE), 1, flags);
                return Term(LocalTerm(data));
            }
            case CLASS_TYPE_VAR: {
                auto id = reader.ReadU8();
                return Term::ClassTypeVariable(id);
            }
            case FUNC_TYPE_VAR: {
                auto id = reader.ReadU8();
                return Term::FuncTypeVariable(id);
            }
            case CANGJIE_ARRAY: {
                auto* data     = AllocateTerm(heap, 1);
                bool isGeneric = false;
                if (!ReadSubTerms(data, &isGeneric, 1, reader)) {
                    return NewUndefined(refId);
                }
                TermFlags flags = {
                    .isLocal       = true,
                    .isReference   = true,
                    .isGeneric     = isGeneric,
                };
                data->InitAfterSubterms(TagTermId(TermKind::CANGJIE_ARRAY), 1, flags);
                return Term(LocalTerm(data));
            }
            case BOX: {
                auto* data     = AllocateTerm(heap, 1);
                bool isGeneric = false;
                if (!ReadSubTerms(data, &isGeneric, 1, reader)) {
                    return NewUndefined(refId);
                }
                TermFlags flags = {
                    .isLocal = true,
                    .isReference = true,
                    .isGeneric = isGeneric,
                };
                data->InitAfterSubterms(TagTermId(TermKind::BOX), 1, flags);
                return Term(LocalTerm(data));
            }
            case FST: {
                auto subtermIdx = reader.ReadULEB();
                auto subterm    = Resolve(RefId<Term>(region, subtermIdx));
                if (subterm.GetKind() == TermKind::UNDEFINED) {
                    return NewUndefined(refId);
                }
                subterm.data->flags.isFixedSize = true;
                return subterm;
            }
            case UNION_ENUM:
            case PRIMITIVE_ENUM:
            case UNION_OPTION:
            case NULLABLE_OPTION:
            case GENERIC_OPTION: {
                auto nameOffs = Offset<String>(reader.ReadULEB());
                auto name = Reader::Read(session, fileId, nameOffs);
                auto arity = reader.ReadU8();
                return ResolveOptionTerm(reader, name, arity, refId, tag);
            }
            default: {
                FATAL("Not implemented for tag %d", tag);
                return NewUndefined(refId);
            }
        }
    }
};

size_t TermManager::InternString(std::string_view str)
{
    std::lock_guard guard(lock);
    return internTable.InternAndGetId(str);
}

Utils::StringPool::String TermManager::GetNameOfAotType(AotTermId type)
{
    std::lock_guard guard(lock);
    return internTable.GetStringById(type.GetNum());
}

Term TermManager::Resolve(Session& session, RefIdentifier<Term> ident)
{
    auto index  = ident.GetIndex();
    auto region = index.GetRegion();
    auto& raf   = session.FileOf(ident.GetFileId());
    auto& file  = session.CbcFileOf(ident.GetFileId());

    auto& manager = TermManager::Of(session);

    TermResolver resolver {
        .regionData = file.GetRegionData(),
        .session    = session,
        .heap       = session.Allocator(),
        .fileId     = ident.GetFileId(),
        .region     = ident.GetIndex().GetRegion(),
        .raf        = *raf,
        .file       = file,
        .manager    = manager,
    };

    // TODO: cache
    return resolver.Resolve(ident.GetIndex());
}

bool Term::IsReference() const { return data->flags.isReference; }

bool Term::IsAotPromoted() const { return data->flags.isAotPromoted; }

bool Term::IsGeneric() const { return data->flags.isGeneric; }

TermFlags Term::Flags() const { return data->flags; }

Term::Range Term::SubTerms() const { return Iterators::MakeRange(Term::SubTermGenerator { data, 0, GetLength() }); }

std::optional<Term> Term::SubTermGenerator::operator()()
{
    if (cursor < end) {
        return term->subterms[cursor++];
    }
    return std::nullopt;
}

Term Substitution::Substitute(Term term)
{
    if (!term.IsGeneric()) {
        return term;
    } else if (term.GetKind() == TermKind::CLASS_TYPE_VAR) {
        return SubstituteClassTv(ClassTvTermId(term).GetNum());
    } else if (term.GetKind() == TermKind::FUNC_TYPE_VAR) {
        return SubstituteFuncTv(FuncTvTermId(term).GetNum());
    } else if (term.GetLength() == 0) {
        return term;
    } else {
        // TODO: cache
        auto data      = term.data;
        auto length    = data->length;
        auto newData   = AllocateTerm(session.Allocator(), length);
        auto flags     = data->flags;
        auto isGeneric = false;
        for (int i = 0; i < length; i++) {
            newData->subterms[i] = Substitute(data->subterms[i]);
            isGeneric            = isGeneric || newData->subterms[i].IsGeneric();
        }
        flags.isLocal   = true;
        flags.isGeneric = isGeneric;
        auto identifier = data->identifier;
        if (identifier.GetKind() == TermKind::GENERIC_OPTION) {
            // Generic option designates an option that wraps a type variable.
            // After substituion, option can change its type to Nullable or Union option.
            auto gIdentifier = GenericOptionId(identifier);
            auto typeDefId = gIdentifier.GetIdentifier();
            auto def = Symlevel::Reader::Read(session, typeDefId);
            auto someType = TermManager::Resolve(session, def->superOrEnumType);

            ClassSubstitution sub(session, newData->subterms, length);
            someType = sub.Substitute(someType);

            if (someType.GetKind() == TermKind::CLASS_TYPE_VAR) {
                identifier = identifier; // no changes
            } else if (someType.GetKind() == TermKind::FUNC_TYPE_VAR) {
                identifier = identifier; // no changes
            } else if (someType.IsReference()) {
                identifier = NullableOptionId(typeDefId);
                flags.isReference = true;
            } else {
                identifier = UnionOptionId(typeDefId);
                flags.isReference = false;
            }
        }
        newData->InitAfterSubterms(identifier, length, flags);
        return LocalTerm(newData);
    }
}

Substitution::Substitution(Session& session) : session(session) {}

ClassSubstitution::ClassSubstitution(Session& session, Term term) : ClassSubstitution(session, term.data->subterms, term.data->length) {}

ClassSubstitution::ClassSubstitution(Session& session, Term* terms, uint32_t termCount) :Substitution(session), terms(terms), termCount(termCount) {}

Term ClassSubstitution::SubstituteClassTv(uint8_t typeVar)
{
    ASSERT(typeVar < termCount);
    return terms[typeVar];
}

Term ClassSubstitution::SubstituteFuncTv(uint8_t typeVar) { return Term::FuncTypeVariable(typeVar); }

ArraySubstitution::ArraySubstitution(Session& session, std::vector<Term> const& terms)
    : Substitution(session),
      terms(terms)
{}

Term ArraySubstitution::SubstituteFuncTv(uint8_t typeVar) { return Term::FuncTypeVariable(typeVar); }

Term ArraySubstitution::SubstituteClassTv(uint8_t typeVar) { return terms.at(typeVar); }

} // namespace Engine
