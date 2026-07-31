#include "code.h"
#include "utils/misc.h"

namespace Symlevel {

Code Code::Resolve(Engine::Session& session, Engine::Identifier<Code> identifier)
{
    return Parse(session, identifier.GetFileId(), identifier.GetOffset());
}

Code Code::Parse(Engine::Session& session, IO::FileId fileId, Offset<Code> offset)
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

std::vector<ExceptionRegion> Code::GetExceptionRegions(Engine::Session& session) const
{
    IO::StreamFileReader reader(*session.FileOf(rawExTable.fileId), rawExTable.start);
    std::vector<ExceptionRegion> regions;
    while (reader.Position() < rawExTable.end) {
        regions.emplace_back(ExceptionRegion {
            .start  = reader.ReadULEB(),
            .end    = reader.ReadULEB(),
            .target = reader.ReadULEB(),
        });
    }
    return regions;
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

        uint32_t slotsN = reader.ReadULEB();
        std::vector<uint32_t> slots;
        slots.reserve(slotsN);

        for (uint32_t idx = 0; idx < slotsN; idx++) {
            slots.push_back(reader.ReadULEB());
        }
        info.refSlotNums = std::move(slots);

        uint32_t pairsN = reader.ReadULEB();
        std::vector<std::pair<uint32_t, uint32_t>> mutPairs;
        mutPairs.reserve(pairsN);

        for (uint32_t idx = 0; idx < pairsN; idx++) {
            mutPairs.push_back(std::pair(reader.ReadULEB(), reader.ReadULEB()));
        }
        info.mutPairs = std::move(mutPairs);

        livenessInfo.push_back(std::move(info));
    }

    return livenessInfo;
}

std::vector<StackPtrsInfo> Code::GetStackPtrsInfo(Engine::Session& session) const
{
    IO::StreamFileReader reader(*session.FileOf(rawStackPtrsInfo.fileId), rawStackPtrsInfo.start);

    std::vector<StackPtrsInfo> stackPtrsInfo;
    while (reader.Position() < rawStackPtrsInfo.end) {
        StackPtrsInfo info = {
            .cbcPos = reader.ReadULEB(),
        };

        uint32_t resourcesN = reader.ReadULEB();
        std::vector<uint32_t> resources;
        resources.reserve(resourcesN);

        for (uint32_t idx = 0; idx < resourcesN; idx++) {
            resources.push_back(reader.ReadULEB());
        }
        info.resources = std::move(resources);

        stackPtrsInfo.push_back(std::move(info));
    }

    return stackPtrsInfo;
}

void Code::Print(Engine::Session& session, Stream::Output& out)
{
    using namespace Stream;
    Stream::Indented out2(out, 2);
    Stream::Indented out4(out, 4);

    out << "MethodCode {" << endl;
    out2 << "untypedSlotCount: " << untypedSlotCount << endl
         << "stackAllocSigsCount: " << stackAllocSigsCount << endl
         << "stackAllocSigs: ";

    for (size_t i = 0; i < stackAllocSigsCount; i++) {
        out2 << stackAllocSigs[i];
        if (i < (stackAllocSigsCount - 1)) {
            out2 << ", ";
        }
    }
    out2 << endl;

    out2 << "ohmSlotCount: " << ohmSlotCount << endl
         << "usedNonVolIRegMask: " << usedNonVolIRegMask << endl
         << "usedNonVolFRegMask: " << usedNonVolFRegMask << endl
         << "maxCalleeStackArgsCount: " << maxCalleeStackArgsCount << endl;

    out2 << "ExceptionTable {" << endl;
    for (const auto& [start, end, target] : GetExceptionRegions(session)) {
        out2 << "  [" << start << ", " << end << ") -> " << target << endl;
    }
    out2 << "}" << endl;

    out2 << "LivenessInfo {" << endl;
    for (const auto& li : GetLivenessInfo(session)) {
        out4 << "cbcPos: " << li.cbcPos << ", regMask: " << li.regMask << ", ";
        Std::Vector::Print(out4, li.refSlotNums);
        out4 << ", ";
        Std::Vector::Print(out4, li.mutPairs);
        out4 << endl;
    }
    out2 << "}" << endl;

    out2 << "StackPtrsInfo {" << endl;
    for (const auto& spi : GetStackPtrsInfo(session)) {
        out4 << "cbcPos: " << spi.cbcPos << ", ";
        Std::Vector::Print(out4, spi.resources);
        out << endl;
    }
    out2 << "}" << endl;

    out << "}" << endl;
}

} // namespace Symlevel
