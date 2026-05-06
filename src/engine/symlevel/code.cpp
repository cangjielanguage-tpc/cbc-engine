#include "code.h"

namespace Symlevel {

Code Code::Parse(Engine::Session& session, IO::FileId fileId, Offset<Code> offset)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetCodeSectionOffs() + offset);

    return Code(session, reader);
}

Code::Code(Engine::Session& session, IO::StreamFileReader& reader)
{
    untypedSlotCount = reader.ReadULEB();
    typedSlotCount   = reader.ReadULEB(); // TODO: impl
    ohmSlotCount     = reader.ReadULEB(); // TODO: impl

    usedNonVolIRegMask      = reader.ReadU8();
    usedNonVolFRegMask      = reader.ReadU8();
    maxCalleeStackArgsCount = reader.ReadULEB();

    mayHaveNativeCalls = static_cast<bool>(reader.ReadU8());
    hasTrivialXHandler = static_cast<bool>(reader.ReadU8());

    codeSize            = reader.ReadULEB();
    auto literalsOffset = reader.ReadULEB(); // TODO: remove

    codePtr = static_cast<uint8_t*>(session.Allocator().Allocate(codeSize, alignof(uint8_t)));
    reader.Read(codePtr, codeSize);

    uint32_t livenessInfoSize = reader.ReadULEB();
    uint32_t livenessStart = reader.Position();

    while (reader.Position() - livenessStart < livenessInfoSize) {
        LivenessInfo info = {
            .cbcPos  = reader.ReadULEB(),
            .regMask = reader.ReadU16(),
        };

        uint32_t n = reader.ReadULEB();
        std::vector<uint32_t> slots;
        slots.reserve(n);
        
        for (uint32_t idx = 0; idx < n; idx++) {
            slots.push_back(reader.ReadULEB());
        }

        info.refSlotNums = std::move(slots);
        livenessInfo.push_back(std::move(info));
    }
}

} // namespace Symlevel
