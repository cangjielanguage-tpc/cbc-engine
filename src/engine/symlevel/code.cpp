#include "code.h"
#include "engine/symlevel/reader.h"
#include "utils/misc.h"

namespace Symlevel {

template <> Code Reader::Read(Engine::Session& session, IO::FileId fileId, Offset<Code> offset)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetCodeSectionOffs() + offset);

    uint32_t untypedSlotCount = reader.ReadULEB();

    uint32_t stackAllocSigsCount = reader.ReadULEB();
    uint32_t* stackAllocSigs =
        static_cast<uint32_t*>(session.Allocator().Allocate(stackAllocSigsCount * sizeof(uint32_t), alignof(uint32_t)));
    for (size_t i = 0; i < stackAllocSigsCount; i++) {
        stackAllocSigs[i] = reader.ReadULEB();
    }

    uint32_t ohmSlotCount = reader.ReadULEB(); // TODO: impl

    uint8_t usedNonVolIRegMask       = reader.ReadU8();
    uint8_t usedNonVolFRegMask       = reader.ReadU8();
    uint32_t maxCalleeStackArgsCount = reader.ReadULEB();

    bool mayHaveNativeCalls = static_cast<bool>(reader.ReadU8());

    uint32_t codeSize       = reader.ReadULEB();
    uint32_t literalsOffset = reader.ReadULEB(); // TODO: remove

    auto codePtr = static_cast<uint8_t*>(session.Allocator().Allocate(codeSize, alignof(uint8_t)));
    reader.Read(codePtr, codeSize);

    uint32_t exTableSize  = reader.ReadULEB();
    uint32_t exTableStart = reader.Position();
    reader.Advance(exTableSize);

    uint32_t livenessInfoSize  = reader.ReadULEB();
    uint32_t livenessInfoStart = reader.Position();
    reader.Advance(livenessInfoSize);

    uint32_t stackPtrsInfoSize  = reader.ReadULEB();
    uint32_t stackPtrsInfoStart = reader.Position();
    reader.Advance(stackPtrsInfoSize);

    return Code(
        untypedSlotCount,
        stackAllocSigsCount,
        stackAllocSigs,
        ohmSlotCount,
        usedNonVolIRegMask,
        usedNonVolFRegMask,
        maxCalleeStackArgsCount,
        mayHaveNativeCalls,
        codeSize,
        codePtr,
        { fileId, exTableStart, exTableStart + exTableSize },
        { fileId, livenessInfoStart, livenessInfoStart + livenessInfoSize },
        { fileId, stackPtrsInfoStart, stackPtrsInfoStart + stackPtrsInfoSize }
    );
}

template <> Code Reader::Read(Engine::Session& session, Engine::Identifier<Code> identifier)
{
    return Reader::Read(session, identifier.GetFileId(), identifier.GetOffset());
}

} // namespace Symlevel
