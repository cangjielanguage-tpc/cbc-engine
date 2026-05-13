#include "code.h"
#include "utils/misc.h"

namespace Symlevel {

Code Code::Parse(Engine::Session& session, IO::FileId fileId, Offset<Code> offset)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetCodeSectionOffs() + offset);

    uint32_t untypedSlotCount = reader.ReadULEB();
    uint32_t typedSlotCount   = reader.ReadULEB(); // TODO: impl
    uint32_t ohmSlotCount     = reader.ReadULEB(); // TODO: impl

    uint8_t usedNonVolIRegMask       = reader.ReadU8();
    uint8_t usedNonVolFRegMask       = reader.ReadU8();
    uint32_t maxCalleeStackArgsCount = reader.ReadULEB();

    bool mayHaveNativeCalls = static_cast<bool>(reader.ReadU8());
    bool hasTrivialXHandler = static_cast<bool>(reader.ReadU8());

    uint32_t codeSize       = reader.ReadULEB();
    uint32_t literalsOffset = reader.ReadULEB(); // TODO: remove

    auto codePtr = static_cast<uint8_t*>(session.Allocator().Allocate(codeSize, alignof(uint8_t)));
    reader.Read(codePtr, codeSize);

    uint32_t livenessInfoSize  = reader.ReadULEB();
    uint32_t livenessInfoStart = reader.Position();
    reader.Advance(livenessInfoSize);

    return Code(
        untypedSlotCount,
        typedSlotCount,
        ohmSlotCount,
        usedNonVolIRegMask,
        usedNonVolFRegMask,
        maxCalleeStackArgsCount,
        mayHaveNativeCalls,
        hasTrivialXHandler,
        codeSize,
        codePtr,
        { fileId, livenessInfoStart, livenessInfoStart + livenessInfoSize }
    );
}

std::vector<LivenessInfo> Code::GetLivenessInfo(Engine::Session& session) const
{
    IO::StreamFileReader reader(*session.FileOf(rawLivenessInfo.fileId), rawLivenessInfo.start);

    std::vector<LivenessInfo> livenessInfo;
    while (reader.Position() < rawLivenessInfo.end) {
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

    return livenessInfo;
}

void Code::Print(Engine::Session& session, Stream::Output& out)
{
    using namespace Stream;
    Stream::Indented out2(out, 2);
    Stream::Indented out4(out, 4);

    out << "MethodCode {" << endl;
    out2 << "untypedSlotCount: " << untypedSlotCount << endl
         << "typedSlotCount: " << typedSlotCount << endl
         << "ohmSlotCount: " << ohmSlotCount << endl
         << "usedNonVolIRegMask: " << usedNonVolIRegMask << endl
         << "usedNonVolFRegMask: " << usedNonVolFRegMask << endl
         << "maxCalleeStackArgsCount: " << maxCalleeStackArgsCount << endl
         << "hasTrivialXHandler: " << hasTrivialXHandler << endl
         << "LivenessInfo {" << endl;

    for (auto& li : GetLivenessInfo(session)) {
        out4 << "cbcPos: " << li.cbcPos << ", regMask: " << li.regMask << ", ";
        Std::Vector::Print(out4, li.refSlotNums);
        out4 << endl;
    }

    out2 << "}" << endl;
    out << "}" << endl;
}

} // namespace Symlevel
