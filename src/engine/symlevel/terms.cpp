#include "terms.h"
#include "definitions.h"
#include "io/stream_file_reader.h"
#include "reader.h"
#include "region_data.h"
#include "string.h"
#include "utils/assertion.h"

namespace Symlevel {

struct TermData {
    TemplateIdentifier identifier;
    uint32_t hash;
    uint16_t length;
    bool isLocal;
    Term subterms[];
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

                auto* data       = static_cast<TermData*>(allocator.Allocate(sizeof(TermData), alignof(TermData)));
                data->identifier = TemplateIdentifier(TemplateKind::TYPE, identifier);
                data->hash       = 0;
                data->length     = 0;
                data->isLocal    = true;

                return Term(LocalTerm(data));
            } else {
                return std::nullopt;
            }
        }

        case AOT_TYPE: {
            auto nameOffs = Offset<String>(reader.ReadULEB());
            auto name     = Reader::Read(session, fileId, nameOffs);

            auto* data       = static_cast<TermData*>(allocator.Allocate(sizeof(TermData), alignof(TermData)));
            data->identifier = TemplateIdentifier(TemplateKind::AOT_TYPE, 0);
            data->hash       = 0;
            data->length     = 0;
            data->isLocal    = true;

            return Term(LocalTerm(data));
        }

        case METHOD_SIGNATURE: {
            auto len = reader.ReadU8() + 1; // +1 for ret type

            auto* data =
                static_cast<TermData*>(allocator.Allocate(sizeof(TermData) + len * sizeof(Term), alignof(TermData)));

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

            data->identifier = TemplateIdentifier(TemplateKind::METHOD);
            data->hash       = 0;
            data->length     = len;
            data->isLocal    = true;

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
    auto* data = static_cast<TermData*>(session.Allocator().Allocate(sizeof(TermData), alignof(TermData)));

    data->identifier = TemplateKind::TYPE;
    data->hash       = 0;
    data->length     = 0;

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

TemplateIdentifier Term::GetIdentifier() const { return data->identifier; }

uint32_t Term::GetLength() const { return data->length; }

bool Term::IsLocal() const { return data->isLocal; }

Term LocalTerm::Subterm(uint32_t i) const { return this->data->subterms[i]; }

LocalTerm::LocalTerm(TermData* data) : data(data) { ASSERT(data->isLocal); }

GlobalTerm GlobalTerm::Subterm(uint32_t i) const { return this->data->subterms[i].AsGlobal(); }

} // namespace Symlevel
