#include "engine/terms.h"
#include "engine/identifiers.h"
#include "engine/symlevel/cbc_file.h"
#include "engine/symlevel/definitions.h"
#include "engine/engine.h"
#include "engine/symlevel/io/file_id.h"
#include "engine/symlevel/io/stream_file_reader.h"
#include "engine/symlevel/reader.h"
#include "engine/symlevel/region_data.h"
#include "string.h"
#include "utils/assertion.h"
#include "utils/heap.h"
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
    TemplateIdentifier identifier;
    uint32_t hash;
    uint16_t length;
    bool isLocal;
    Term subterms[];

    void InitAfterSubterms(TemplateIdentifier identifier, uint16_t length, bool isLocal)
    {
        Init(identifier, identifier.Hash() ^ hash, length, isLocal);
    }

    void Init(TemplateIdentifier identifier, uint32_t hash, uint16_t length, bool isLocal)
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
    return static_cast<TermData*>(
        allocator.Allocate(sizeof(TermData) + subtermCount * sizeof(Term), alignof(TermData))
    );
}

TypeTemplateIdentifier TemplateIdentifier::AsTypeIdent() { return TypeTemplateIdentifier(ident); }

AotTypeTemplateIdentifier TemplateIdentifier::AsAotIdent() { return AotTypeTemplateIdentifier(ident); }

TagTemplateIdentifier TemplateIdentifier::AsTagIdent() { return TagTemplateIdentifier(ident); }

UndefinedTemplateIdentifier TemplateIdentifier::AsUndefinedIdent() { return UndefinedTemplateIdentifier(ident); }

// NOTE: the order is the same as the order of builtin terms in cbc format.
static TermData builtins[] = {
    { TagTemplateIdentifier(TemplateKind::NIL), 0xa0, 0, false },
    { TagTemplateIdentifier(TemplateKind::VOID), 0xa1, 0, false },
    { TagTemplateIdentifier(TemplateKind::UNIT), 0xa2, 0, false },
    { TagTemplateIdentifier(TemplateKind::NOTHING), 0xb3, 0, false },
    { TagTemplateIdentifier(TemplateKind::BOOLEAN), 0xb4, 0, false },
    { TagTemplateIdentifier(TemplateKind::I8), 0xb5, 0, false },
    { TagTemplateIdentifier(TemplateKind::U8), 0xc6, 0, false },
    { TagTemplateIdentifier(TemplateKind::I16), 0xc7, 0, false },
    { TagTemplateIdentifier(TemplateKind::U16), 0xd8, 0, false },
    { TagTemplateIdentifier(TemplateKind::I32), 0xd9, 0, false },
    { TagTemplateIdentifier(TemplateKind::U32), 0x10, 0, false },
    { TagTemplateIdentifier(TemplateKind::UCHAR32), 0x41, 0, false },
    { TagTemplateIdentifier(TemplateKind::I64), 0x32, 0, false },
    { TagTemplateIdentifier(TemplateKind::U64), 0x23, 0, false },
    { TagTemplateIdentifier(TemplateKind::IADDR), 0x14, 0, false },
    { TagTemplateIdentifier(TemplateKind::UADDR), 0x45, 0, false },
    { TagTemplateIdentifier(TemplateKind::BSTRING), 0x16, 0, false },
    { TagTemplateIdentifier(TemplateKind::F16), 0x87, 0, false },
    { TagTemplateIdentifier(TemplateKind::F32), 0x98, 0, false },
    { TagTemplateIdentifier(TemplateKind::F64), 0x29, 0, false },
};

static Term Primitive(Session& session, Symlevel::Index<Term> index)
{
    static_assert(FIRST_NON_PRIMITIVE == sizeof(builtins) / sizeof(builtins[0]));
    int num = static_cast<int>(index.GetIndex());
    ASSERT(num < FIRST_NON_PRIMITIVE);
    return GlobalTerm(&builtins[num]);
}

Term Term::Definition(Session& session, Identifier<Symlevel::TypeDefinition> type)
{
    // TODO: assertions for length
    auto* data = AllocateTerm(session.Allocator());
    data->InitAfterSubterms(TypeTemplateIdentifier(type), 0, true);
    return LocalTerm(data);
}

static Term Undefined(Session& session, IndexIdentifier<Term> termId)
{
    // TODO: assertions for length
    auto* data = AllocateTerm(session.Allocator());
    data->InitAfterSubterms(UndefinedTemplateIdentifier(termId), 0, true);
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

TemplateIdentifier Term::GetIdentifier() const { return data->identifier; }

uint32_t Term::GetLength() const { return data->length; }

uint32_t Term::Hash() const { return data->hash; }

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

    Term NewUndefined(Symlevel::Index<Term> index) {
        return Undefined(session, IndexIdentifier(index, fileId));
    }

    Term Resolve(Symlevel::Index<Term> index)
    {
        using namespace Symlevel;

        if (index.GetIndex() < FIRST_NON_PRIMITIVE) {
            return Primitive(session, index);
        }
        auto offset = regionData.Query(session, index);
        IO::StreamFileReader reader(raf, file.GetTermSectionOffs() + offset);

        auto tag = static_cast<Tag>(reader.ReadU8());
        switch (tag) {
            case TYPE: {
                auto name = Reader::Read(session, fileId, Offset<String>(reader.ReadULEB()));
                // FIXME: must be option of identifier, not of TypeDefinition itself
                auto type = session.GetEngine().FindType(session, name);
                if (!type.has_value()) {
                    return NewUndefined(index);
                }
                auto identifier = type.value().GetIdentifier();
                auto* data      = AllocateTerm(heap);
                data->InitAfterSubterms(TypeTemplateIdentifier(identifier), 0, true);
                return Term(LocalTerm(data));
            }
            case AOT_TYPE: {
                auto nameOffs = Offset<String>(reader.ReadULEB());
                auto* data    = AllocateTerm(heap);
                data->InitAfterSubterms(AotTypeTemplateIdentifier(nameOffs, fileId), 0, true);
                return Term(LocalTerm(data));
            }
            case METHOD_SIGNATURE: {
                auto len   = reader.ReadU8() + 1; // +1 for ret type
                auto* data = AllocateTerm(heap, len);

                auto& regionData = session.CbcFileOf(fileId).GetRegionData();
                for (int i = 0; i < len; i++) {
                    auto subtermIdx = reader.ReadULEB();
                    auto subterm    = Resolve(Index<Term>(index.GetRegion(), subtermIdx));
                    if (subterm.GetIdentifier().GetKind() == TemplateKind::UNDEFINED) {
                        return NewUndefined(index);
                    }
                }

                data->InitAfterSubterms(TagTemplateIdentifier(TemplateKind::METHOD), len, true);
                return Term(LocalTerm(data));
            }
            default: {
                FATAL("Not implemented for tag %d", tag);
                return NewUndefined(index);
            }
        }
    }
};

Term TermManager::Resolve(Session& session, IndexIdentifier<Term> ident)
{
    auto index = ident.GetIndex();
    auto region = index.GetRegion();
    auto& raf = session.FileOf(ident.GetFileId());
    auto& file = session.CbcFileOf(ident.GetFileId());

    TermResolver resolver { file.GetRegionData(), session, session.Allocator(), ident.GetFileId(), *raf, file };

    return resolver.Resolve(ident.GetIndex());
}

} // namespace Symlevel
