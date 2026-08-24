#include "references.h"
#include "engine/symlevel/flags.h"
#include "engine/terms.h"
#include "io/stream_file_reader.h"
#include "region_data.h"
#include <cstdint>

namespace Symlevel {

MethodReference ParseReference(Engine::Session& session, IO::FileId fileId, Offset<MethodReference> offset)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetMethodRefSectionOffs() + offset);

    auto nameOffset   = Engine::Identifier<String>(Offset<String>(reader.ReadU32()), fileId);
    auto parsedFlags  = reader.ReadU8();
    auto refTypeIdx   = Engine::RefIdentifier(RefId<Term>(reader.ReadULEB()), fileId);
    auto methodSigIdx = Engine::RefIdentifier(RefId<Term>(reader.ReadULEB()), fileId);

    MethodRefFlags flags;
    if (parsedFlags & 0x1) flags = flags.Or(MethodRefFlag::SRET);
    if (parsedFlags & 0x2) flags = flags.Or(MethodRefFlag::HAS_THIS_TI);
    if (parsedFlags & 0x4) flags = flags.Or(MethodRefFlag::HAS_OUTER_TI);
    if (parsedFlags & 0x8)
        flags = flags.Or(MethodRefFlag::MUT);
    if (parsedFlags & 0x10) flags = flags.Or(MethodRefFlag::HAS_FTVARS);
    if (parsedFlags & 0x20) flags = flags.Or(MethodRefFlag::AOT);
    if (parsedFlags & 0x40)
        flags = flags.Or(MethodRefFlag::REC_RECEIVER);
    if (parsedFlags & 0x80)
        flags = flags.Or(MethodRefFlag::REF_RECEIVER);

    static constexpr auto NIL_ID = RefId<Term>((uint16_t)Engine::TermKind::NIL);

    Engine::RefIdentifier<Term> tvars(NIL_ID, fileId);
    if (flags.Is(MethodRefFlag::HAS_FTVARS)) {
        tvars = Engine::RefIdentifier(RefId<Term>(reader.ReadULEB()), fileId);
    }

    return { nameOffset, refTypeIdx, methodSigIdx, tvars, flags };
}

template <typename Reference>
inline static Reference ParseReference(Engine::Session& session, Engine::RefIdentifier<Reference> identifier);

FieldReference ParseReference(Engine::Session& session, IO::FileId fileId, Offset<FieldReference> offset)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetFieldRefSectionOffs() + offset);

    uint8_t tag = reader.ReadU8();
    switch (tag) {
        case SINGLE: {
            auto nameOffset   = Engine::Identifier<String>(Offset<String>(reader.ReadU32()), fileId);
            auto refTypeIdx   = Engine::RefIdentifier(RefId<Term>(reader.ReadULEB()), fileId);
            auto fieldTypeIdx = Engine::RefIdentifier(RefId<Term>(reader.ReadULEB()), fileId);
            return FieldReference(nameOffset, refTypeIdx, fieldTypeIdx);
        }
        case CONST_INDEX: {
            auto idx          = reader.ReadU32();
            auto refTypeIdx   = Engine::RefIdentifier(RefId<Term>(reader.ReadULEB()), fileId);
            auto fieldTypeIdx = Engine::RefIdentifier(RefId<Term>(reader.ReadULEB()), fileId);
            return FieldReference(idx, refTypeIdx, fieldTypeIdx);
        }
        case MULTI: {
            uint32_t length = reader.ReadULEB();
            std::vector<FieldReference> fieldRefs;
            std::vector<RefId<FieldReference>> indices;
            fieldRefs.reserve(length);
            indices.reserve(length);

            for (uint32_t i = 0; i < length; i++) {
                auto id    = RefId<FieldReference>(reader.ReadULEB());
                auto ident = Engine::RefIdentifier(id, fileId);
                auto ref   = ParseReference(session, ident);
                fieldRefs.push_back(ref);
                indices.push_back(id);
            }

            void* subRefsPtr  = session.Allocator().Allocate(length * sizeof(FieldReference), alignof(FieldReference));
            auto subRefsStart = reinterpret_cast<FieldReference*>(subRefsPtr);
            std::memcpy(subRefsStart, fieldRefs.data(), length * sizeof(FieldReference));

            void* indicesPtr  = session.Allocator().Allocate(length * sizeof(RefId<FieldReference>), alignof(uint32_t));
            auto indicesStart = reinterpret_cast<RefId<FieldReference>*>(indicesPtr);
            std::memcpy(indicesStart, indices.data(), length * sizeof(RefId<FieldReference>));

            return FieldReference(length, subRefsStart, indicesStart);
        }
        case NONE: {
            auto sig = Engine::RefIdentifier(RefId<Term>(reader.ReadULEB()), fileId);
            return FieldReference(sig);
        }
        default: FATAL("Unknown field reference tag: %d", tag);
    }
}

template <typename Reference>
inline static Reference ParseReference(Engine::Session& session, Engine::RefIdentifier<Reference> identifier)
{
    auto& file       = session.CbcFileOf(identifier.GetFileId());
    auto& raf        = session.FileOf(identifier.GetFileId());
    auto& regionData = file.GetRegionData();
    auto offset      = regionData.Query(session, identifier.GetIndex());
    return ParseReference(session, identifier.GetFileId(), offset);
}

MethodReference MethodReference::Parse(Engine::Session& session, Engine::RefIdentifier<MethodReference> identifier)
{
    return ParseReference(session, identifier);
}

FieldReference FieldReference::Parse(Engine::Session& session, Engine::RefIdentifier<FieldReference> identifier)
{
    return ParseReference(session, identifier);
}

} // namespace Symlevel
