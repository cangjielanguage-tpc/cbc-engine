#include "terms.h"
#include "definitions.h"
#include "engine/arena.h"
#include "engine/engine.h"
#include "io/stream_file_reader.h"
#include "reader.h"
#include "region_data.h"
#include "string.h"
#include "utils/assertion.h"
#include <alloca.h>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <unordered_set>

namespace Symlevel {

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

std::optional<Term> Term::ParseAndResolve(Engine::Session& session, IO::FileId fileId, Offset<Term> offset)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetTermSectionOffs() + offset);
    auto& allocator = session.Allocator();

    auto tag = static_cast<Tag>(reader.ReadU8());
    switch (tag) {
        case TYPE: {
            auto nameOffs = Offset<String>(reader.ReadULEB());
            auto name     = Reader::Read(session, fileId, nameOffs);
            auto type     = session.GetEngine().FindType(session, name);
            if (type.has_value()) {
                auto identifier = type.value().GetIdentifier();

                auto* data = AllocateTerm(allocator);
                data->InitAfterSubterms(TemplateIdentifier(TemplateKind::TYPE, identifier), 0, true);

                return Term(LocalTerm(data));
            } else {
                return std::nullopt;
            }
        }

        case AOT_TYPE: {
            auto nameOffs = Offset<String>(reader.ReadULEB());
            auto name     = Reader::Read(session, fileId, nameOffs);

            auto* data = AllocateTerm(allocator);
            data->InitAfterSubterms(TemplateIdentifier(TemplateKind::AOT_TYPE, 0), 0, true);

            return Term(LocalTerm(data));
        }

        case METHOD_SIGNATURE: {
            auto len = reader.ReadU8() + 1; // +1 for ret type
            auto* data = AllocateTerm(allocator, len);

            auto& regionData = session.CbcFileOf(fileId).GetRegionData();
            for (int i = 0; i < len; i++) {
                auto subtermIdx = reader.ReadULEB();
                auto subterm    = regionData.queryTerm(session, { .region = 0, .index = subtermIdx });
                if (subterm.has_value()) {
                    data->subterms[i] = subterm.value();
                } else {
                    allocator.Free(data, sizeof(TermData) + len * sizeof(Term), alignof(TermData));
                    return std::nullopt;
                }
            }

            data->InitAfterSubterms(TemplateIdentifier(TemplateKind::METHOD), len, true);

            return Term(LocalTerm(data));
        }

        default: {
            ASSERTION(false, "not implemented");
            return std::nullopt;
        }
    }
}

static TermData builtins[] = {
    { TemplateKind::NIL, 0xa0, 0, false },     { TemplateKind::VOID, 0xa1, 0, false },
    { TemplateKind::UNIT, 0xa2, 0, false },    { TemplateKind::NOTHING, 0xb3, 0, false },
    { TemplateKind::BOOLEAN, 0xb4, 0, false }, { TemplateKind::I8, 0xb5, 0, false },
    { TemplateKind::U8, 0xc6, 0, false },      { TemplateKind::I16, 0xc7, 0, false },
    { TemplateKind::U16, 0xd8, 0, false },     { TemplateKind::I32, 0xd9, 0, false },
    { TemplateKind::U32, 0x10, 0, false },     { TemplateKind::UCHAR32, 0x41, 0, false },
    { TemplateKind::I64, 0x32, 0, false },     { TemplateKind::U64, 0x23, 0, false },
    { TemplateKind::IADDR, 0x14, 0, false },   { TemplateKind::UADDR, 0x45, 0, false },
    { TemplateKind::BSTRING, 0x16, 0, false }, { TemplateKind::F16, 0x87, 0, false },
    { TemplateKind::F32, 0x98, 0, false },     { TemplateKind::F64, 0x29, 0, false },
};

Term Term::Builtin(Engine::Session& session, TemplateKind kind)
{
    return GlobalTerm(&builtins[static_cast<int>(kind)]);
}

Term Term::Definition(Engine::Session& session, Engine::Identifier<TypeDefinition> type)
{
    // TODO: assertions for length
    auto* data = AllocateTerm(session.Allocator());
    data->InitAfterSubterms(TemplateIdentifier(TemplateKind::TYPE, type), 0, true);

    return LocalTerm(data);
}

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

bool Term::operator==(const Term& another) const { return CompareTermData(this->data, another.data, false); }

bool Term::IsLocal() const { return data->isLocal; }

Term LocalTerm::Subterm(uint32_t i) const { return this->data->subterms[i]; }
LocalTerm::LocalTerm(TermData* data) : data(data) { ASSERT(data->isLocal); }

GlobalTerm LocalTerm::Publish(Engine::Session& session)
{
    Term term(*this);
    return TermManager::Of(session).Globalize(term);
}

GlobalTerm GlobalTerm::Subterm(uint32_t i) const { return this->data->subterms[i].AsGlobal(); }

TermManager::TermManager() : impl(std::make_unique<Impl>()) {}

TermManager::TermManager(TermManager&& manager) = default;
TermManager::~TermManager()                     = default;

struct CacheEntry {
    TermData* data;

    bool operator==(const CacheEntry& another) const { return CompareTermData(data, another.data, true); }
};

struct TermManager::Impl {
    struct Hasher {
        uint64_t operator()(CacheEntry const& entry) const { return entry.data->hash; }
    };

    std::unordered_set<CacheEntry, Hasher> set;
};

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

    // query cache without allocating a new term
    CacheEntry findEntry { termData };

    auto it = impl->set.find(findEntry);
    if (it != impl->set.end()) {
        return GlobalTerm(it->data);
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

    impl->set.insert(CacheEntry { data });
    term.data = data;
    return term.AsGlobal();
}

} // namespace Symlevel
