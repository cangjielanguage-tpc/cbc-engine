#include "engine/terms.h"
#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/symlevel/cbc_file.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/io/file_id.h"
#include "engine/symlevel/io/stream_file_reader.h"
#include "engine/symlevel/reader.h"
#include "engine/symlevel/region_data.h"
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

/// Internal representation of `Term`.
/// The main things which are needed to represent term is an identifier and subterms.
/// The length of subterm array is bounded by 2^16, so in the leftover memory
/// we fit additional fields `hash` and `isLocal`.
struct TermData {
    TermId identifier;
    uint32_t hash;
    uint16_t length;
    bool isLocal;
    Term subterms[];

    void InitAfterSubterms(TermId identifier, uint16_t length, bool isLocal)
    {
        Init(identifier, identifier.Hash() ^ hash, length, isLocal);
    }

    void Init(TermId identifier, uint32_t hash, uint16_t length, bool isLocal)
    {
        this->identifier = identifier;
        this->isLocal    = isLocal;
        this->length     = length;
        this->hash       = hash;
    }
};

enum Tag : uint8_t {
    NIL,                      // 0x00
    TYPE,                     // 0x01
    AOT_TYPE,                 // 0x02
    CANGJIE_ARRAY,            // 0x03
    VARRAY,                   // 0x04
    ENUM_WRAPPER,             // 0x05
    C_POINTER,                // 0x06
    GENERIC_TYPE_TERM,        // 0x07
    GENERIC_TYPE_VAR,         // 0x08
    GENERIC_RECORD,           // 0x09
    GENERIC_REFERENCE,        // 0x0a
    NULLABLE,                 // 0x0b
    METHOD_SIGNATURE,         // 0x0c
    GENERIC_METHOD,           // 0x0d
    CONSTRAINT,               // 0x0e
    PARAMETERIZED_CONSTRAINT, // 0x0f
    JAVA_REFERENCE,           // 0x10
    JAVA_ARRAY,               // 0x11
    NON_NULLABLE,             // 0x12
};

static TermData* AllocateTerm(Memory::Heap& allocator, size_t subtermCount = 0)
{
    return static_cast<TermData*>(allocator.Allocate(sizeof(TermData) + subtermCount * sizeof(Term), alignof(TermData))
    );
}

// NOTE: the order is the same as the order of builtin terms in cbc format.
static TermData builtins[] = {
    { TagTermId(TermKind::NIL), 0xa0, 0, false },     { TagTermId(TermKind::VOID), 0xa1, 0, false },
    { TagTermId(TermKind::UNIT), 0xa2, 0, false },    { TagTermId(TermKind::NOTHING), 0xb3, 0, false },
    { TagTermId(TermKind::BOOLEAN), 0xb4, 0, false }, { TagTermId(TermKind::I8), 0xb5, 0, false },
    { TagTermId(TermKind::U8), 0xc6, 0, false },      { TagTermId(TermKind::I16), 0xc7, 0, false },
    { TagTermId(TermKind::U16), 0xd8, 0, false },     { TagTermId(TermKind::I32), 0xd9, 0, false },
    { TagTermId(TermKind::U32), 0x10, 0, false },     { TagTermId(TermKind::UCHAR32), 0x41, 0, false },
    { TagTermId(TermKind::I64), 0x32, 0, false },     { TagTermId(TermKind::U64), 0x23, 0, false },
    { TagTermId(TermKind::IADDR), 0x14, 0, false },   { TagTermId(TermKind::UADDR), 0x45, 0, false },
    { TagTermId(TermKind::BSTRING), 0x16, 0, false }, { TagTermId(TermKind::F16), 0x87, 0, false },
    { TagTermId(TermKind::F32), 0x98, 0, false },     { TagTermId(TermKind::F64), 0x29, 0, false },
};
static_assert(FIRST_NON_PRIMITIVE == sizeof(builtins) / sizeof(builtins[0]));

bool TermId::IsReference()
{
    switch (GetKind()) {
        case TermKind::AOT_TYPE:
        case TermKind::TYPE:
        case TermKind::NULLABLE:
        case TermKind::NON_NULLABLE:
        case TermKind::CANGJIE_ARRAY:
        case TermKind::TYPE_VAR:      return true;
        default:                      return false;
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
    ASSERT(num < FIRST_NON_PRIMITIVE);
    return GlobalTerm(&builtins[num]);
}

Term Term::Definition(Session& session, Identifier<Symlevel::TypeDefinition> type)
{
    // TODO: assertions for length
    auto* data = AllocateTerm(session.Allocator());
    data->InitAfterSubterms(TypeTermId(type), 0, true);
    return LocalTerm(data);
}

static Term Undefined(Session& session, RefIdentifier<Term> termId)
{
    // TODO: assertions for length
    auto* data = AllocateTerm(session.Allocator());
    data->InitAfterSubterms(UndefTermId(termId), 0, true);
    return LocalTerm(data);
}

static bool CompareTermData(TermData* origin, TermData* another, bool ignoreLocal)
{
    if (another == origin) {
        return true;
    } else if (another->hash != origin->hash) {
        return false;
    } else if (!ignoreLocal && another->isLocal != origin->isLocal) {
        return false;
    } else if (another->identifier != origin->identifier) {
        return false;
    } else if (another->length != origin->length) {
        return false;
    } else {
        auto length = origin->length;
        for (auto i = 0; i < length; i++) {
            if (!CompareTermData(another->subterms[i].data, origin->subterms[i].data, ignoreLocal)) {
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
            // FIXME: separate ret type from rest
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
            auto ident = AotTermId(*this).GetIdentifier();
            stream << Symlevel::String::Parse(session, ident.GetFileId(), ident.GetOffset());
            if (int len = GetLength(); len > 0) {
                printSubTerms("<", ">", len);
            }
            break;
        }

        case TK::TYPE_VAR: {
            stream << "$Tunimplemented";
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

bool Term::operator==(const Term& another) const { return CompareTermData(this->data, another.data, false); }

bool Term::IsLocal() const { return data->isLocal; }

Term LocalTerm::Subterm(uint32_t i) const { return this->data->subterms[i]; }

LocalTerm::LocalTerm(TermData* data) : data(data) { ASSERT(data->isLocal); }

GlobalTerm LocalTerm::Publish(Session& session)
{
    Term term(*this);
    return TermManager::Of(session).Globalize(term);
}

GlobalTerm GlobalTerm::Subterm(uint32_t i) const { return this->data->subterms[i].AsGlobal(); }

bool GlobalTerm::operator==(const GlobalTerm& another) const { return this == &another; }

bool GlobalTerm::operator!=(const GlobalTerm& another) const { return this != &another; }

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
    data->Init(termData->identifier, termData->hash, termData->length, false);

    cache.insert(data);
    term.data = data;
    return term.AsGlobal();
}

uint64_t TermManager::Hasher::operator()(TermData* const& data) const { return data->hash; }

struct TermResolver {
    Symlevel::RegionData const& regionData;
    Session& session;
    Memory::Heap& heap;
    IO::FileId fileId;
    IO::RandomAccessFile& raf;
    Symlevel::CbcFile& file;

    Term NewUndefined(Symlevel::RefId<Term> refId) { return Undefined(session, RefIdentifier(refId, fileId)); }

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
            case TYPE: {
                auto name = Reader::Read(session, fileId, Offset<String>(reader.ReadULEB()));
                auto type = session.GetEngine().FindType(session, name);
                if (!type.has_value()) {
                    return NewUndefined(refId);
                }
                auto identifier = type.value();
                auto* data      = AllocateTerm(heap);
                data->InitAfterSubterms(TypeTermId(identifier), 0, true);
                return Term(LocalTerm(data));
            }
            case AOT_TYPE: {
                auto nameOffs   = Offset<String>(reader.ReadULEB());
                auto* data      = AllocateTerm(heap);
                auto identifier = Identifier(nameOffs, fileId);
                data->InitAfterSubterms(AotTermId(identifier), 0, true);
                return Term(LocalTerm(data));
            }
            case METHOD_SIGNATURE: {
                auto len   = reader.ReadU8() + 1; // +1 for ret type
                auto* data = AllocateTerm(heap, len);

                auto& regionData = session.CbcFileOf(fileId).GetRegionData();
                for (int i = 0; i < len; i++) {
                    auto subtermIdx = reader.ReadULEB();
                    auto subterm    = Resolve(RefId<Term>(refId.GetRegion(), subtermIdx));
                    if (subterm.GetId().GetKind() == TermKind::UNDEFINED) {
                        return NewUndefined(refId);
                    }
                    data->subterms[subtermIdx] = subterm;
                }

                data->InitAfterSubterms(TagTermId(TermKind::METHOD), len, true);
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

    TermResolver resolver { file.GetRegionData(), session, session.Allocator(), ident.GetFileId(), *raf, file };

    return resolver.Resolve(ident.GetIndex());
}

} // namespace Engine
