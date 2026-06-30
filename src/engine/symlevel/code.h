#pragma once

#include "cbc/isa.h"
#include "engine/engine.h"
#include "utils/ostream.h"

namespace Symlevel {

struct ExceptionRegion {
    uint32_t start;
    uint32_t end;
    uint32_t target;
};

struct RawExceptionTable {
    IO::FileId fileId;
    uint32_t start;
    uint32_t end;
};

struct LivenessInfo {
    uint32_t cbcPos;
    uint16_t regMask;
    std::vector<uint32_t> refSlotNums;
    std::vector<std::pair<uint32_t, uint32_t>> mutPairs;
};

struct RawLivenessInfo {
    IO::FileId fileId;
    uint32_t start;
    uint32_t end;
};

class Code {
public:
    static Code Resolve(Engine::Session& session, Engine::Identifier<Code> identifier);

    static Code Parse(Engine::Session& session, IO::FileId fileId, Offset<Code> offset);

    static Code Mock(uint8_t* codePtr, uint32_t codeSize) { return Code(codePtr, codeSize); }

    uint8_t* CodePtr() { return codePtr; }

    uint32_t CodeSize() { return codeSize; }

    uint32_t UntypedSlotCount() { return untypedSlotCount; }

    uint32_t StackAllocSigsCount() { return stackAllocSigsCount; }

    uint32_t* StackAllocSigs() { return stackAllocSigs; }

    uint8_t UsedNonVolIRegMask() { return usedNonVolIRegMask; }

    uint8_t UsedNonVolFRegMask() { return usedNonVolFRegMask; }

    std::vector<ExceptionRegion> GetExceptionRegions(Engine::Session& session) const;

    std::vector<LivenessInfo> GetLivenessInfo(Engine::Session& session) const;

    void Print(Engine::Session& session, Stream::Output& out);

private:
    Code(uint8_t* codePtr, uint32_t codeSize) : codePtr(codePtr), codeSize(codeSize) {}

    Code(
        uint32_t untypedSlotCount,
        uint32_t stackAllocSigsCount,
        uint32_t* stackAllocSigs,
        uint32_t ohmSlotCount,
        uint8_t usedNonVolIRegMask,
        uint8_t usedNonVolFRegMask,
        uint32_t maxCalleeStackArgsCount,
        bool mayHaveNativeCalls,
        uint32_t codeSize,
        uint8_t* codePtr,
        RawExceptionTable rawExTable,
        RawLivenessInfo rawLivenessInfo
    )
        : untypedSlotCount(untypedSlotCount),
          stackAllocSigsCount(stackAllocSigsCount),
          stackAllocSigs(stackAllocSigs),
          ohmSlotCount(ohmSlotCount),
          usedNonVolIRegMask(usedNonVolIRegMask),
          usedNonVolFRegMask(usedNonVolFRegMask),
          maxCalleeStackArgsCount(maxCalleeStackArgsCount),
          mayHaveNativeCalls(mayHaveNativeCalls),
          codePtr(codePtr),
          codeSize(codeSize),
          rawExTable(rawExTable),
          rawLivenessInfo(rawLivenessInfo)
    {}

    uint32_t untypedSlotCount    = 0;
    uint32_t stackAllocSigsCount = 0;
    uint32_t ohmSlotCount        = 0;

    uint32_t* stackAllocSigs = nullptr;

    uint8_t usedNonVolIRegMask       = 0;
    uint8_t usedNonVolFRegMask       = 0;
    uint32_t maxCalleeStackArgsCount = 0;

    bool mayHaveNativeCalls = false;

    uint32_t codeSize;
    uint8_t* codePtr;

    RawExceptionTable rawExTable = { IO::FileId(0), 0, 0 };

    RawLivenessInfo rawLivenessInfo = { IO::FileId(0), 0, 0 };
};

} // namespace Symlevel
