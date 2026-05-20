#include "engine/terms.h"
#include "engine/engine.h"
#include "engine/identifiers.h"
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
#include "utils/ostream.h"
#include <alloca.h>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_set>

namespace Engine {

struct TermFlags {
    uint16_t isLocal       : 1;
    uint16_t isReference   : 1;
    uint16_t isAotPromoted : 1;
    uint16_t isGeneric     : 1;

    TermFlags() = delete;
};

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
    NIL = 0x0,
    REF = 0x1,
    AOT_REF = 0x2,
    CANGJIE_ARRAY = 0x3,
    VARRAY = 0x4,
    ENUM_WRAPPER = 0x5,
    C_POINTER = 0x6,
    FUNC_TYPE_VAR = 0x7,
    CLASS_TYPE_VAR = 0x8,
    GENERIC_RECORD = 0x9,
    GENERIC_REFERENCE = 0xa,
    NULLABLE = 0xb,
    METHOD_SIGNATURE = 0xc,
    REC = 0xd,
    AOT_REC = 0xe,
    NON_NULLABLE = 0xf,
    GENERIC_AOT_REF = 0x10,
    GENERIC_AOT_REC = 0x11,
};

static TermData* AllocateTerm(Memory::Heap& allocator, size_t subtermCount = 0)
{
    return static_cast<TermData*>(
        allocator.Allocate(sizeof(TermData) + subtermCount * sizeof(Term), alignof(TermData))
    );
}

struct BuiltinTerms {
    void* memory;
    void* primitives;
    void* classTypeVars;
    void* funcTypeVars;

    static constexpr size_t TV_COUNT = 256;
    static constexpr size_t PRIM_COUNT = FIRST_NON_PRIMITIVE;

    BuiltinTerms(BuiltinTerms const&) = delete;

    ~BuiltinTerms() {
        std::free(memory);
    }

    inline static TermData* DataAt(void *memory, size_t idx) {
        char* ptr = reinterpret_cast<char*>(memory);
        ptr += sizeof(TermData) * idx;
        return reinterpret_cast<TermData*>(ptr);
    }

    inline TermData* Primitive(size_t i) const {
        ASSERT(i < PRIM_COUNT);
        return DataAt(primitives, i);
    }

    inline TermData* ClassTv(size_t i) const {
        ASSERT(i < TV_COUNT);
        return DataAt(classTypeVars, i);
    }

    inline TermData* FuncTv(size_t i) const {
        ASSERT(i < TV_COUNT);
        return DataAt(funcTypeVars, i);
    }

    static BuiltinTerms Create() {

        size_t seed = 0xf123123a;

        auto hash = [&seed]() {
            constexpr size_t multiplier = 3202034522624059733L;
            constexpr size_t addend = 0x421L;
            auto next = (multiplier * seed + addend);
            return seed = next;
        };

        void* memory = std::malloc(sizeof(TermData) * (TV_COUNT + TV_COUNT + PRIM_COUNT));
        if (!memory) FATAL("Failed to allocate builtin terms");

        void* primitives = DataAt(memory, 0);
        void* classTypeVars = DataAt(memory, PRIM_COUNT);
        void* funcTypeVars = DataAt(memory, PRIM_COUNT + TV_COUNT);

        TermFlags primFlags = {
            .isLocal = false,
            .isReference = false,
            .isAotPromoted = false,
            .isGeneric = false,
        };

        TermFlags tvFlags = {
            .isLocal = false,
            .isReference = true,
            .isAotPromoted = false,
            .isGeneric = true,
        };

        for (size_t i = 0; i < PRIM_COUNT; i++) {
            auto kind = TermKind(i);
            auto data = DataAt(primitives, i);
            data->hash = hash();
            data->length = 0;
            data->identifier = TagTermId(kind);
            data->flags = primFlags;
        }

        for (size_t i = 0; i < TV_COUNT; i++) {
            auto data = DataAt(classTypeVars, i);
            data->hash = hash();
            data->length = 0;
            data->identifier = ClassTvTermId(i);
            data->flags = tvFlags;
        }

        for (size_t i = 0; i < TV_COUNT; i++) {
            auto data = DataAt(funcTypeVars, i);
            data->hash = hash();
            data->length = 0;
            data->identifier = FuncTvTermId(i);
            data->flags = tvFlags;
        }

        return { memory, primitives, classTypeVars, funcTypeVars };
    }
};

static BuiltinTerms const& Builtins() {
    static auto instance = BuiltinTerms::Create();
    return instance;
}

bool TermId::IsReference()
{
    switch (GetKind()) {
        case TermKind::AOT_TYPE:
        case TermKind::TYPE:
        case TermKind::NULLABLE:
        case TermKind::NON_NULLABLE:
        case TermKind::CANGJIE_ARRAY:
        case TermKind::FUNC_TYPE_VAR:
        case TermKind::CLASS_TYPE_VAR: return true;

        default: return false;
    }
}

int TermId::Width()
{
    switch (GetKind()) {
        case TermKind::BOOLEAN:
        case TermKind::U8:
        case TermKind::I8:        return sizeof(uint8_t);
        case TermKind::U16:
        case TermKind::I16:
        case TermKind::F16:       return sizeof(uint16_t);
        case TermKind::U32:
        case TermKind::I32:
        case TermKind::F32:
        case TermKind::UCHAR32:   return sizeof(uint32_t);
        case TermKind::U64:
        case TermKind::I64:
        case TermKind::F64:
        case TermKind::C_POINTER:
        case TermKind::IADDR:
        case TermKind::UADDR:     return sizeof(uint64_t);
        default:
            if (IsReference()) {
                return sizeof(uint64_t);
            }
            FATAL("Not supported yet %d", GetKind());
            return 0;
    }
}

Term Term::Predefined(TermKind tk)
{
    int num = static_cast<int>(tk);
    return GlobalTerm(Builtins().Primitive(num));
}

Term Term::ClassTypeVariable(uint8_t tv)
{
    return GlobalTerm(Builtins().ClassTv(tv));
}

Term Term::FuncTypeVariable(uint8_t tv)
{
    return GlobalTerm(Builtins().FuncTv(tv));
}

Term Term::Definition(Session& session, Identifier<Symlevel::TypeDefinition> type)
{
    // TODO: handle arity and generic type vars
    auto def   = Symlevel::Reader::Read(session, type);
    bool isRec = def.GetFlags().Is(Symlevel::TypeKind::RECORD);
    auto* data = AllocateTerm(session.Allocator());
    data->InitAfterSubterms(TypeTermId(type), 0, {
        .isLocal = true,
        .isReference = !isRec,
        .isAotPromoted = false,
    });
    return LocalTerm(data);
}

static Term Undefined(Session& session, RefIdentifier<Term> termId)
{
    // TODO: assertions for length
    auto* data = AllocateTerm(session.Allocator());
    data->InitAfterSubterms(UndefTermId(termId), 0, {
        .isLocal = true,
        .isReference = !false,
        .isAotPromoted = false,
    });
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

Term::Term(LocalTerm local) : data(local.data) {}

Term::Term(GlobalTerm global) : data(global.data) {}

Term::Term(Term const& term) : data(term.data) {}

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

uint32_t Term::GetLength() const { return data->length; }

uint32_t Term::Hash() const { return data->hash; }

std::string Term::GetName(Session& session) const
{
    Stream::StringBuffer buf;
    GetName(session, buf);
    return buf.ToString();
}

void Term::GetName(Session& session, Stream::Output& stream) const
{
    auto printSubTerms = [&](std::string_view prefix, std::string_view suffix, int len) {
        stream << prefix;
        auto separator = "";
        for (int i = 0; i < len; i++) {
            stream << separator;
            Subterm(i).GetName(session, stream);
            separator = ", ";
        }
        stream << suffix;
    };

    using TK = TermKind;
    switch (GetKind()) {
        case TK::NIL:     stream << "nil"; break;
        case TK::VOID:    stream << "void"; break;
        case TK::UNIT:    stream << "unit"; break;
        case TK::NOTHING: stream << "nothing"; break;
        case TK::BOOLEAN: stream << "bool"; break;
        case TK::I8:      stream << "i8"; break;
        case TK::U8:      stream << "u8"; break;
        case TK::I16:     stream << "i16"; break;
        case TK::U16:     stream << "u16"; break;
        case TK::I32:     stream << "i32"; break;
        case TK::U32:     stream << "u32"; break;
        case TK::UCHAR32: stream << "uchar32"; break;
        case TK::I64:     stream << "i64"; break;
        case TK::U64:     stream << "u64"; break;
        case TK::IADDR:   stream << "iaddr"; break;
        case TK::UADDR:   stream << "uaddr"; break;
        case TK::BSTRING: stream << "bstr"; break;
        case TK::F16:     stream << "f16"; break;
        case TK::F32:     stream << "f32"; break;
        case TK::F64:     stream << "f64"; break;

        case TK::UNDEFINED: {
            auto undef  = UndefTermId(*this).GetIdentifier();
            auto file   = undef.GetFileId();
            auto region = undef.GetIndex().GetRegion();
            auto index  = undef.GetIndex().GetIndex();
            stream.PrintFmt("$unresolved<%u,%u,%u>", file.id, region, index);
            break;
        }

        case TK::C_POINTER: {
            stream << "$cpointer<";
            Subterm(0).GetName(session, stream);
            stream << '>';
            break;
        }

        case TK::NULLABLE: {
            stream << "$nullable<";
            Subterm(0).GetName(session, stream);
            stream << '>';
            break;
        }

        case TK::CANGJIE_ARRAY: {
            stream << "$array<";
            Subterm(0).GetName(session, stream);
            stream << '>';
            break;
        }

        case TK::METHOD: {
            printSubTerms("(", ")", GetLength() - 1);
            Subterm(GetLength() - 1).GetName(session, stream);
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

        case TK::AOT_TYPE:
        case TK::AOT_REC: {
            auto ident = AotTermId(*this).GetIdentifier();
            stream << Symlevel::String::Parse(session, ident.GetFileId(), ident.GetOffset());
            if (int len = GetLength(); len > 0) {
                printSubTerms("<", ">", len);
            }
            break;
        }

        case TK::CLASS_TYPE_VAR: {
            auto tv = ClassTvTermId(*this).GetNum();
            stream << "$CT" << tv;
            break;
        }

        case TK::FUNC_TYPE_VAR: {
            auto tv = FuncTvTermId(*this).GetNum();
            stream << "$FT" << tv;
            break;
        }

        case TK::GENERIC_METHOD: {
            stream << "$GMunimplemented";
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

Term LocalTerm::Subterm(uint32_t i) const { return this->data->subterms[i]; }

LocalTerm::LocalTerm(TermData* data) : data(data) { ASSERT(data->flags.isLocal); }

GlobalTerm LocalTerm::Publish(Session& session)
{
    Term term(*this);
    return TermManager::Of(session).Globalize(term);
}

GlobalTerm GlobalTerm::Subterm(uint32_t i) const { return this->data->subterms[i].AsGlobal(); }

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
        throw std::bad_alloc();
    }

    for (int i = 0; i < term.GetLength(); i++) {
        data->subterms[i] = termData->subterms[i];
    }
    auto flags = termData->flags;
    flags.isLocal = false;
    data->Init(termData->identifier, termData->hash, termData->length, flags);

    cache.insert(data);
    term.data = data;
    return term.AsGlobal();
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

    Term NewUndefined(Symlevel::RefId<Term> refId) { return Undefined(session, RefIdentifier(refId, fileId)); }

    bool ReadSubTerms(TermData* data, bool* isGenericLoc, int length, IO::StreamFileReader& reader) {
        using namespace Symlevel;
        bool isGeneric = false;

        for (int i = 0; i < length; i++) {
            auto subtermIdx = reader.ReadULEB();
            auto subterm    = Resolve(RefId<Term>(region, subtermIdx));
            if (subterm.GetKind() == TermKind::UNDEFINED) {
                return false;
            }
            data->subterms[i] = subterm;
            isGeneric = isGeneric || subterm.IsGeneric();
        }
        *isGenericLoc = isGeneric;
        return true;
    }

    Term ResolveTypeDefTerm(IO::StreamFileReader& reader, Symlevel::Offset<Symlevel::String> nameOffs, int expectedLength, bool isReference, Symlevel::RefId<Term> refId, bool wasAot)
    {
        using namespace Symlevel;

        auto name = Reader::Read(session, fileId, nameOffs);
        auto type = session.GetEngine().FindType(session, name);
        if (!type.has_value()) {
            return NewUndefined(refId);
        }
        auto identifier = type.value();

        auto def = Symlevel::TypeDefinition::Resolve(session, identifier);
        bool undefined = false;
        if ((def.GetFlags().Is(Symlevel::TypeKind::RECORD)) == isReference) {
            undefined = true;
        } else if (def->arity != expectedLength) {
            undefined = true;
        }

        if (undefined && wasAot) {
            FATAL("Unexpected mismatch of resolved type definition and aot term");
        }
        if (undefined) {
            return NewUndefined(refId);
        }

        auto data = AllocateTerm(heap, expectedLength);
        bool isGeneric = false;
        if (!ReadSubTerms(data, &isGeneric, expectedLength, reader)) {
            return NewUndefined(refId);
        }

        data->InitAfterSubterms(TypeTermId(identifier), 0, {
            .isLocal = true,
            .isReference = isReference,
            .isAotPromoted = wasAot,
            .isGeneric = isGeneric,
        });
        return Term(LocalTerm(data));
    }

    Term ResolveAotType(IO::StreamFileReader& reader, Symlevel::Offset<Symlevel::String> nameOffs, int length, bool isReference, Symlevel::RefId<Term> refId) {
        // Attempt to find type definition, even if the type is tagged as aot.
        // Because the type could present in `TypeDefinition` super closure
        // or be present as "patch".
        auto term = ResolveTypeDefTerm(reader, nameOffs, length, isReference, refId, true);
        if (term.GetKind() != TermKind::UNDEFINED) {
            return term;
        }

        auto data = AllocateTerm(heap, length);
        bool isGeneric = false;
        if (!ReadSubTerms(data, &isGeneric, length, reader)) {
            return NewUndefined(refId);
        }
        TermFlags flags = {
            .isLocal = true,
            .isReference = isReference,
            .isAotPromoted = false,
            .isGeneric = isGeneric,
        };
        // FIXME: in multi-cbc scenario this identifier is not unique.
        if (flags.isReference) {
            data->InitAfterSubterms(AotTermId(Identifier(nameOffs, fileId)), 0, flags);
        } else {
            data->InitAfterSubterms(AotRecTermId(Identifier(nameOffs, fileId)), 0, flags);
        }
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
                return ResolveTypeDefTerm(reader, nameOffs, length, tag == REF, refId, false);
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
            case METHOD_SIGNATURE: {
                auto len   = reader.ReadU8() + 1; // +1 for ret type
                auto* data = AllocateTerm(heap, len);

                bool isGeneric = false;
                if (!ReadSubTerms(data, &isGeneric, len, reader)) {
                    return NewUndefined(refId);
                }
                TermFlags flags = {
                    .isLocal = true,
                    .isReference = false,
                    .isAotPromoted = false,
                    .isGeneric = isGeneric,
                };
                data->InitAfterSubterms(TagTermId(TermKind::METHOD), len, flags);
                return Term(LocalTerm(data));
            }
            case NULLABLE: {
                auto* data     = AllocateTerm(heap, 1);
                bool isGeneric = false;
                if (!ReadSubTerms(data, &isGeneric, 1, reader)) {
                    return NewUndefined(refId);
                }

                TermFlags flags = {
                    .isLocal = true,
                    .isReference = true,
                    .isAotPromoted = false,
                    .isGeneric = isGeneric,
                };
                data->InitAfterSubterms(TagTermId(TermKind::NULLABLE), 1, flags);
                return Term(LocalTerm(data));
            }
            default: {
                FATAL("Not implemented for tag %d", tag);
                return NewUndefined(refId);
            }
        }
    }
};

Term TermManager::Resolve(Session& session, RefIdentifier<Term> ident)
{
    auto index  = ident.GetIndex();
    auto region = index.GetRegion();
    auto& raf   = session.FileOf(ident.GetFileId());
    auto& file  = session.CbcFileOf(ident.GetFileId());

    TermResolver resolver {
        .regionData = file.GetRegionData(),
        .session = session,
        .heap = session.Allocator(),
        .fileId = ident.GetFileId(),
        .region = ident.GetIndex().GetRegion(),
        .raf= *raf,
        .file = file
    };

    // TODO: cache
    return resolver.Resolve(ident.GetIndex());
}

bool Term::IsReference() const { return data->flags.isReference; }

bool Term::IsAotPromoted() const { return data->flags.isAotPromoted; }

bool Term::IsGeneric() const { return data->flags.isGeneric; }

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
        auto data = term.data;
        auto length = data->length;
        auto newData = AllocateTerm(session.Allocator(), length);
        auto flags = data->flags;
        flags.isLocal = true;
        flags.isGeneric = false;
        for (int i = 0; i < length; i++) {
            newData->subterms[i] = Substitute(data->subterms[i]);
        }
        newData->InitAfterSubterms(data->identifier, length, flags);
        return LocalTerm(newData);
    }
}

Substitution::Substitution(Session& session) : session(session) {}

ClassSubstitution::ClassSubstitution(Session& session, Term term) : Substitution(session), term(term) {}

Term ClassSubstitution::SubstituteClassTv(uint8_t typeVar)
{
    ASSERT(typeVar < term.GetLength());
    return term.Subterm(typeVar);
}

Term ClassSubstitution::SubstituteFuncTv(uint8_t typeVar)
{
    return Term::FuncTypeVariable(typeVar);
}

} // namespace Engine
