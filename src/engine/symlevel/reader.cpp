#include "reader.h"

namespace Symlevel {

std::vector<ExceptionRegion> Reader::GetExceptionRegions(Engine::Session& session, Code const& code)
{
    IO::StreamFileReader reader(*session.FileOf(code.rawExTable.fileId), code.rawExTable.start);
    std::vector<ExceptionRegion> regions;
    while (reader.Position() < code.rawExTable.end) {
        regions.emplace_back(ExceptionRegion {
            .start  = reader.ReadULEB(),
            .end    = reader.ReadULEB(),
            .target = reader.ReadULEB(),
        });
    }
    return regions;
}

std::vector<LivenessInfo> Reader::GetLivenessInfo(Engine::Session& session, Code const& code)
{
    auto& rawLivenessInfo = code.rawLivenessInfo;
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

std::vector<StackPtrsInfo> Reader::GetStackPtrsInfo(Engine::Session& session, Code const& code)
{
    auto& rawStackPtrsInfo = code.rawStackPtrsInfo;
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
} // namespace Symlevel
