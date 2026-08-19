#include "engine/terms.h"
#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/image/cbc_file.h"
#include "engine/image/io/stream_file_reader.h"
#include "engine/image/reader.h"
#include "engine/image/type_kind.h"
#include "engine/resolving_output.h"
#include "string.h"
#include "utils/assertion.h"
#include "utils/heap.h"
#include "utils/iterators.h"
#include "utils/ostream.h"
#include <alloca.h>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_set>

namespace Engine {

/// Internal representation of `Term`.
/// The main things which are needed to represent term is an identifier and subterms.
/// The length of subterm array is bounded by 2^16, so in the leftover memory
/// we fit additional fields `hash` and `flags`.
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
    ENUM_WRAPPER      = 0x5, // TODO: delete
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
    OPTION            = 0x15,
    UNION_ENUM        = 0x16,
    PRIMITIVE_ENUM    = 0x17,
};

static TermData* AllocateTerm(Memory::Heap& allocator, size_t subtermCount = 0)
{
    return static_cast<TermData*>(allocator.Allocate(sizeof(TermData) + subtermCount * sizeof(Term), alignof(TermData))
    );
}

constexpr TermFlags::TermFlags(int flags)
    : isLocal((flags & F_LOCAL) != 0),
      isReference((flags & F_REFERENCE) != 0),
      isAotPromoted((flags & F_AOT_PROMOTED) != 0),
      isGeneric((flags & F_GENERIC) != 0),
      isFixedSize((flags & F_FST) != 0),
      isRecord((flags & F_RECORD) != 0)
{}

TermFlags TermFlags::operator+=(TermFlags flags)
{
    isLocal       |= flags.isLocal;
    isReference   |= flags.isReference;
    isAotPromoted |= flags.isAotPromoted;
    isGeneric     |= flags.isGeneric;
    isFixedSize   |= flags.isFixedSize;
    isRecord      |= flags.isRecord;
    return *this;
}

struct BuiltinTerms {
    static constexpr size_t TV_COUNT   = 256;
    static constexpr size_t PRIM_COUNT = FIRST_NON_PRIMITIVE;

    static inline char primitives[sizeof(TermData) * PRIM_COUNT];
    static inline char classTypeVars[sizeof(TermData) * TV_COUNT];
    static inline char funcTypeVars[sizeof(TermData) * TV_COUNT];

    BuiltinTerms(BuiltinTerms const&) = delete;

    static TermData* DataAt(char* ptr, size_t idx)
    {
        ptr += sizeof(TermData) * idx;
        return reinterpret_cast<TermData*>(ptr);
    }

    static TermData* Primitive(size_t i)
    {
        ASSERT(i < PRIM_COUNT);
        return DataAt(primitives, i);
    }

    static TermData* ClassTv(size_t i)
    {
        ASSERT(i < TV_COUNT);
        return DataAt(classTypeVars, i);
    }

    static TermData* FuncTv(size_t i)
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

        TermFlags tvFlags   = F_REFERENCE | F_GENERIC;

        for (size_t i = 0; i < PRIM_COUNT; i++) {
            auto kind        = TermKind(i);
            auto data        = DataAt(primitives, i);
            data->hash       = hash();
            data->length     = 0;
            data->identifier = TagTermId(kind);
            data->flags      = kind == TermKind::UNIT ? F_RECORD : 0;
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

Term Term::Definition(Session& session, Identifier<Image::TypeDefinition> type)
{
    // TODO: handle arity and generic type vars
    auto def   = Image::Reader::Read(session, type);
    bool isRec = def.GetFlags().Is(Image::TypeKind::RECORD);
    auto arity = def->arity;

    auto* data = AllocateTerm(session.Allocator(), arity);
    for (uint8_t i = 0; i < arity; i++) {
        data->subterms[i] = ClassTypeVariable(i);
    }
    TermFlags flags   = F_LOCAL;
    flags.isReference = !isRec;
    flags.isRecord    = isRec;
    flags.isGeneric   = (arity > 0);
    data->InitAfterSubterms(TypeTermId(type), arity, flags);
    return LocalTerm(data);
}

static Term Undefined(Session& session, RefIdentifier<Term> termId)
{
    // TODO: assertions for length
    auto* data = AllocateTerm(session.Allocator());
    data->InitAfterSubterms(UndefTermId(termId), 0, F_LOCAL);
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

class TermPrinter : public Stream::ResolvingOutput {
public:
    TermPrinter(Session& session, Stream::Output& out, bool hasDebugPrefix)
        : Stream::ResolvingOutput(session, out),
          hasDebugPrefix(hasDebugPrefix)
    {}

    bool hasDebugPrefix;

    TermPrinter& operator<<(Term term)
    {
        term.GetName(session, out, hasDebugPrefix);
        return *this;
    }

    template <typename T> TermPrinter& operator<<(T val)
    {
        out << val;
        return *this;
    }
};

void Term::GetName(Session& session, Stream::Output& out, bool hasDebugPrefix) const
{
    TermPrinter stream(session, out, hasDebugPrefix);
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

    auto prefix = [](TK tk, bool hasDebugPrefix) {
        if (!hasDebugPrefix)
            return "";
        switch (tk) {
            case TermKind::TYPE:           return "@";
            case TermKind::AOT_TYPE:       return "#";
            case TermKind::UNION_ENUM:     return "^";
            case TermKind::OPTION:         return "?";
            case TermKind::PRIMITIVE_ENUM: return "~";
            default:                       return "";
        }
    }(kind, hasDebugPrefix);

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
        case TK::UCHAR32: stream << "Rune"; break;
        case TK::I64:     stream << "Int64"; break;
        case TK::U64:     stream << "UInt64"; break;
        case TK::IADDR:   stream << "IntNative"; break;
        case TK::UADDR:   stream << "UIntNative"; break;
        case TK::BSTRING: stream << "BString"; break;
        case TK::F16:     stream << "Float16"; break;
        case TK::F32:     stream << "Float32"; break;
        case TK::F64:     stream << "Float64"; break;

        case TK::UNDEFINED: {
            auto undef  = UndefTermId(*this).GetIdentifier();
            auto file   = undef.GetFileId();
            auto index  = undef.GetIndex();
            out.PrintFmt("$unresolved<%u,%u>", file.id, index);
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
            stream << "RawArray<" << Subterm(0) << '>';
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

        case TK::UNION_ENUM:
        case TK::OPTION:
        case TK::PRIMITIVE_ENUM:
        case TK::TYPE: {
            auto ident = ExtractTypeDefIdentifier(*this);
            auto type  = Image::Reader::Read(session, ident);
            stream << prefix << Image::Reader::Read(session, type.GetName());
            if (int len = GetLength(); len > 0) {
                printSubTerms("<", ">", len);
            }
            break;
        }

        case TK::AOT_TYPE: {
            auto& manager = TermManager::Of(session);
            std::string_view name = manager.GetNameOfAotType(AotTermId(*this));
            stream << prefix << name;
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

static int OptionFlags(RefIdentifier<Term> underlyingRef, Session& session, Substitution& sub)
{
    auto underlying = TermManager::Resolve(session, underlyingRef);

    underlying = sub.Substitute(underlying);
    auto kind  = underlying.GetKind();

    // Option of nullable-option is not nullable-option.
    bool canBeNullableOption = (kind == TermKind::TYPE || kind == TermKind::AOT_TYPE);
    return canBeNullableOption && underlying.IsReference() ? F_REFERENCE : F_RECORD;
}

static bool IsProperTypeReference(Image::TypeDefinition& def, bool isReference, int arity)
{
    if ((def.GetFlags().Is(Image::TypeKind::RECORD)) == isReference) {
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
    TermFlags flags   = F_LOCAL;
    flags.isReference = isReference;
    flags.isRecord    = !isReference;
    flags.isGeneric   = isGeneric;

    auto type = session.GetEngine().FindType(session, name);
    if (type.has_value()) {
        ASSERT([&]() -> bool {
            auto def = Image::Reader::Read(session, type.value());
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

static Term NewTermWithId(Session& session, TermId id, bool isReference, Term const* subterms, size_t termCount)
{
    auto& heap     = session.Allocator();
    auto data      = AllocateTerm(heap, termCount);
    bool isGeneric = false;
    auto arity     = termCount;
    for (int i = 0; i < arity; i++) {
        data->subterms[i] = subterms[i];
        isGeneric         = isGeneric || subterms[i].IsGeneric();
    }

    TermFlags flags   = F_LOCAL;
    flags.isReference = isReference;
    flags.isRecord    = !isReference;
    flags.isGeneric   = isGeneric;
    data->InitAfterSubterms(id, arity, flags);
    return Term(LocalTerm(data));
}

Term TermManager::NewTermWithId(Session& session, TermId id, bool isReference, std::vector<Term> const& subterms)
{
    return ::Engine::NewTermWithId(session, id, isReference, subterms.data(), subterms.size());
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
    } else if (left->identifier != right->identifier) {
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
    Image::RegionData const& regionData;
    Session& session;
    Memory::Heap& heap;
    FileId fileId;
    IO::RandomAccessFile& raf;
    Image::CbcFile& file;
    TermManager& manager;

    Term NewUndefined(Image::RefId<Term> refId) { return Undefined(session, Image::RefIdentifier(refId, fileId)); }

    bool ReadSubTerms(TermData* data, bool* isGenericLoc, int length, IO::StreamFileReader& reader)
    {
        using namespace Image;
        bool isGeneric = false;

        for (int i = 0; i < length; i++) {
            auto subtermIdx = reader.ReadULEB();
            auto subterm    = Resolve(RefId<Term>(subtermIdx));
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
        Image::Offset<Image::String> nameOffs,
        int expectedLength,
        bool isReference,
        Image::RefId<Term> refId,
        bool wasAot
    )
    {
        using namespace Image;

        auto name = Reader::Read(session, fileId, nameOffs);
        return ResolveTypeDefTerm(reader, name, expectedLength, isReference, refId, wasAot);
    }

    Term ResolveTypeDefTerm(
        IO::StreamFileReader& reader,
        Image::String name,
        int expectedLength,
        bool isReference,
        Image::RefId<Term> refId,
        bool wasAot
    )
    {
        using namespace Image;

        auto type = session.GetEngine().FindType(session, name);
        if (!type.has_value()) {
            return NewUndefined(refId);
        }
        auto identifier = type.value();

        auto def       = Image::Reader::Read(session, identifier);
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

        TermFlags flags     = F_LOCAL;
        flags.isReference   = isReference;
        flags.isRecord      = !isReference;
        flags.isGeneric     = isGeneric;
        flags.isAotPromoted = wasAot;

        data->InitAfterSubterms(TypeTermId(identifier), expectedLength, flags);
        return Term(LocalTerm(data));
    }

    Term ResolveEnumTerm(
        IO::StreamFileReader& reader, Image::String name, int expectedLength, Image::RefId<Term> refId, Tag tag
    )
    {
        using namespace Image;

        auto type = session.GetEngine().FindType(session, name);
        if (!type.has_value()) {
            return NewUndefined(refId);
        }
        auto identifier = type.value();

        auto def = Image::Reader::Read(session, identifier);

        bool optionLikeEnum = false;
        switch (def->enumKind) {
            case Image::EnumKind::OPTION0:
            case Image::EnumKind::OPTION1: optionLikeEnum = true;
            default: {}
        }

        bool undefined = false;
        TermId id = TagTermId(TermKind::NIL);
        switch (tag) {
            case OPTION: {
                undefined = !optionLikeEnum;
                id          = OptionId(identifier);
                break;
            }
            case UNION_ENUM:
                undefined = def->enumKind != Image::EnumKind::UNION;
                id = UnionEnumId(identifier);
                break;
            case PRIMITIVE_ENUM:
                undefined = def->enumKind != Image::EnumKind::PRIMITIVE;
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

        TermFlags flags = F_LOCAL;
        flags.isGeneric = isGeneric;
        if (tag == UNION_ENUM) {
            flags += F_RECORD;
        }
        if (tag == OPTION) {
            ClassSubstitution sub(session, data->subterms, expectedLength);
            flags += OptionFlags(def.GetEnumType(), session, sub);
        }

        data->InitAfterSubterms(id, expectedLength, flags);
        return Term(LocalTerm(data));
    }

    Term ResolveAotType(
        IO::StreamFileReader& reader,
        Image::Offset<Image::String> nameOffs,
        int length,
        bool isReference,
        Image::RefId<Term> refId
    )
    {
        auto name = Image::Reader::Read(session, fileId, nameOffs);
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
        TermFlags flags   = F_LOCAL;
        flags.isReference = isReference;
        flags.isRecord    = !flags.isReference;
        flags.isGeneric   = isGeneric;
        auto internedName = manager.InternString(name);

        data->InitAfterSubterms(AotTermId(internedName), length, flags);
        return Term(LocalTerm(data));
    }

    Term NewTerm(IO::StreamFileReader& reader, Image::RefId<Term> refId, TermId id, uint16_t length, TermFlags flags)
    {
        auto* data = AllocateTerm(heap, length);

        bool isGeneric = false;
        if (!ReadSubTerms(data, &isGeneric, length, reader)) {
            return NewUndefined(refId);
        }
        flags.isGeneric = isGeneric;
        data->InitAfterSubterms(id, length, flags);
        return Term(LocalTerm(data));
    }

    Term Resolve(Image::RefId<Term> refId)
    {
        using namespace Image;

        if (refId < FIRST_NON_PRIMITIVE) {
            return Term::Predefined(TermKind(refId.GetValue()));
        }

        auto pool  = file.GetRegionData().template ErasedPool<Term>();
        auto index = refId - pool.adjustment;
        assert(index < pool.size);

        auto offset = raf.ReadU32(pool.offset + index * sizeof(uint32_t));
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
                return NewTerm(reader, refId, TagTermId(TermKind::FUNCTIONAL), len, F_LOCAL | F_REFERENCE);
            }
            case TUPLE: {
                auto len   = reader.ReadULEB();
                return NewTerm(reader, refId, TagTermId(TermKind::TUPLE), len, F_LOCAL | F_RECORD);
            }
            case NULLABLE: {
                return NewTerm(reader, refId, TagTermId(TermKind::NULLABLE), 1, F_LOCAL | F_REFERENCE);
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
                return NewTerm(reader, refId, TagTermId(TermKind::CANGJIE_ARRAY), 1, F_LOCAL | F_REFERENCE);
            }
            case C_POINTER: {
                return NewTerm(reader, refId, TagTermId(TermKind::C_POINTER), 1, F_LOCAL);
            }
            case BOX: {
                return NewTerm(reader, refId, TagTermId(TermKind::BOX), 1, F_LOCAL | F_REFERENCE);
            }
            case FST: {
                auto subtermIdx = reader.ReadULEB();
                auto subterm    = Resolve(RefId<Term>(subtermIdx));
                if (subterm.GetKind() == TermKind::UNDEFINED) {
                    return NewUndefined(refId);
                }
                subterm.data->flags.isFixedSize = true;
                return subterm;
            }
            case UNION_ENUM:
            case PRIMITIVE_ENUM:
            case OPTION: {
                auto nameOffs = Offset<String>(reader.ReadULEB());
                auto name = Reader::Read(session, fileId, nameOffs);
                auto arity = reader.ReadU8();
                return ResolveEnumTerm(reader, name, arity, refId, tag);
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
    auto& raf   = session.FileOf(ident.GetFileId());
    auto& file  = session.CbcFileOf(ident.GetFileId());

    auto& manager = TermManager::Of(session);

    TermResolver resolver {
        .regionData = file.GetRegionData(),
        .session    = session,
        .heap       = session.Allocator(),
        .fileId     = ident.GetFileId(),
        .raf        = *raf,
        .file       = file,
        .manager    = manager,
    };

    // TODO: cache
    return resolver.Resolve(ident.GetIndex());
}

bool Term::IsReference() const { return data->flags.isReference; }

static bool CheckIsRecord(TermKind kind, TermData* data)
{
    switch (kind) {
        case TermKind::UNIT:
        case TermKind::TUPLE:
        case TermKind::UNION_ENUM: return true;
        case TermKind::TYPE:
        case TermKind::OPTION:
        case TermKind::AOT_TYPE:   return !data->flags.isReference;
        default:                   return false;
    }
}

bool Term::IsRecord() const
{
    ASSERT(CheckIsRecord(GetKind(), data) == data->flags.isRecord);
    return data->flags.isRecord;
}

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

        depth++;
        for (int i = 0; i < length; i++) {
            newData->subterms[i] = Substitute(data->subterms[i]);
            isGeneric            = isGeneric || newData->subterms[i].IsGeneric();
        }
        depth--;

        if (term.GetKind() == TermKind::OPTION) {
            auto id = ExtractTypeDefIdentifier(term);
            auto def = Image::Reader::Read(session, id);
            ClassSubstitution sub(session, data->subterms, length);
            flags += OptionFlags(def.GetEnumType(), session, sub);
        }
        flags.isLocal   = true;
        flags.isGeneric = isGeneric;
        newData->InitAfterSubterms(data->identifier, length, flags);
        return LocalTerm(newData);
    }
}

Substitution::Substitution(Session& session) : session(session) {}

ClassSubstitution::ClassSubstitution(Session& session, Term term) : ClassSubstitution(session, term.data->subterms, term.data->length) {}

ClassSubstitution::ClassSubstitution(Session& session, std::vector<Term> const& terms)
    : ClassSubstitution(session, terms.data(), terms.size())
{}

ClassSubstitution::ClassSubstitution(Session& session, Term const* terms, size_t size)
    : Substitution(session),
      terms(terms),
      size(size)
{}

Term ClassSubstitution::SubstituteFuncTv(uint8_t typeVar) { return Term::FuncTypeVariable(typeVar); }

Term ClassSubstitution::SubstituteClassTv(uint8_t typeVar)
{
    ASSERT(typeVar < size);
    return terms[typeVar];
}

MethodSignatureSubstitution::MethodSignatureSubstitution(Session& session, Term term)
    : MethodSignatureSubstitution(session, term.data->subterms, term.data->length)
{}

MethodSignatureSubstitution::MethodSignatureSubstitution(Session& session, Term const* terms, size_t size)
    : Substitution(session),
      sub(session, terms, size)
{}

Term MethodSignatureSubstitution::SubstituteFuncTv(uint8_t typeVar) { return Term::FuncTypeVariable(typeVar); }

Term MethodSignatureSubstitution::SubstituteClassTv(uint8_t typeVar)
{
    auto substituted = sub.SubstituteClassTv(typeVar);
    if (depth == 1 && !substituted.IsReference()) {
        // To prevent method resolution ambiguity, outermost type variables which
        // are substituted as records/primitives must be wrapped as boxes.
        // depth == 0 -> method signature itself
        // depth == 1 -> method signature arguments
        Term subterms[] = { substituted };
        substituted     = NewTermWithId(session, TagTermId(TermKind::BOX), true, subterms, 1);
    }
    return substituted;
}

Identifier<Image::TypeDefinition> ExtractTypeDefIdentifier(Term term)
{
    switch (term.GetKind()) {
        case TermKind::UNION_ENUM:      return UnionEnumId(term).GetIdentifier();
        case TermKind::OPTION:          return OptionId(term).GetIdentifier();
        case TermKind::PRIMITIVE_ENUM:  return PrimitiveEnumId(term).GetIdentifier();
        case TermKind::TYPE:            return TypeTermId(term).GetIdentifier();
        default:                        FATAL("unexpected kind %d", term.GetKind());
    }
}

} // namespace Engine
