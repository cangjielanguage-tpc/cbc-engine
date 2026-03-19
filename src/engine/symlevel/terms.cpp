#include "terms.h"
#include "definitions.h"
#include "engine/identifiers.h"
#include "index.h"
#include "io/stream_file_reader.h"
#include "reader.h"
#include "region_data.h"

namespace Symlevel {
namespace Terms {

enum Tag : uint8_t {
    NIL,                      // 0x00
    RECORD,                   // 0x01
    REFERENCE,                // 0x02
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
    AOT_TYPE                  // 0x13
};

std::optional<Term> Term::ParseAndResolve(Engine::Session& session, IO::FileId fileId, Offset<Term> offset)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetTermSectionOffs() + offset);
    auto& allocator = session.Allocator();

    auto tag = static_cast<Tag>(reader.ReadU8());
    switch (tag) {
        case REFERENCE:
        case AOT_TYPE:  {
            auto nameOffs                      = Offset<String>(reader.ReadULEB());
            auto name                          = Reader::Read(session, fileId, nameOffs);
            std::optional<TypeDefinition> type = session.GetEngine().FindType(session, name);
            if (type.has_value()) {
                auto identifier = type.value().GetIdentifier();
                auto* data      = static_cast<TermData*>(allocator.Allocate(sizeof(TermData), alignof(TermData)));

                TemplateKind kind = tag == REFERENCE ? TemplateKind::TYPE : TemplateKind::AOT_TYPE;
                data->identifier  = TemplateIdentifier(kind, identifier);
                data->hash        = 0;
                data->length      = 0;

                return Term(LocalTerm(data));
            } else {
                return std::nullopt;
            }
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

            return Term(LocalTerm(data));
        }

        default: {
            ASSERTION(false, "not implemented");
            return std::nullopt;
        }
    }
}

Term Term::Builtin(Engine::Session& session, TemplateKind kind)
{
    auto* data = static_cast<TermData*>(session.Allocator().Allocate(sizeof(TermData), alignof(TermData)));

    data->identifier = TemplateKind(kind);
    data->hash       = 0;
    data->length     = 0;

    // TODO: Global?
    return LocalTerm(data);
}

LocalTerm Term::AsLocal()
{
    ASSERT(IsLocal());
    return std::get<LocalTerm>(this->term);
}

GlobalTerm Term::AsGlobal()
{
    ASSERT(!IsLocal());
    return std::get<GlobalTerm>(this->term);
}

TemplateIdentifier Term::GetIdentifier() const
{
    return std::visit([](auto&& t) { return t.data->identifier; }, term);
}

uint32_t Term::GetLength() const
{
    return std::visit([](auto&& t) { return t.data->length; }, term);
}

bool Term::IsLocal() const { return std::holds_alternative<LocalTerm>(term); }

Term LocalTerm::Subterm(uint32_t i) const { return this->data->subterms[i]; }

GlobalTerm GlobalTerm::Subterm(uint32_t i) const { return this->data->subterms[i].AsGlobal(); }

} // namespace Terms
} // namespace Symlevel
