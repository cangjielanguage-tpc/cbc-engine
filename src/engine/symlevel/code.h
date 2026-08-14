#pragma once

#include "engine/symlevel/io/file_id.h"
#include <vector>

namespace Symlevel {

struct ExceptionRegion {
    uint32_t start;
    uint32_t end;
    uint32_t target;
};

struct RawData {
    IO::FileId fileId;
    uint32_t start;
    uint32_t end;
};

// Remove heap-allocated fields in case of moving those structures allocation into arena
struct LivenessInfo {
    uint32_t cbcPos;
    uint16_t regMask;
    std::vector<uint32_t> refSlotNums;
    std::vector<std::pair<uint32_t, uint32_t>> mutPairs;
};

struct StackPtrsInfo {
    uint32_t cbcPos;
    std::vector<uint32_t> resources;
};

class Code {
public:
    static Code Mock(uint8_t* codePtr, uint32_t codeSize) { return Code(codePtr, codeSize); }

    uint8_t* CodePtr() { return codePtr; }

    uint32_t CodeSize() { return codeSize; }

    uint32_t UntypedSlotCount() { return untypedSlotCount; }

    uint32_t StackAllocSigsCount() { return stackAllocSigsCount; }

    uint32_t* StackAllocSigs() { return stackAllocSigs; }

    uint8_t UsedNonVolIRegMask() { return usedNonVolIRegMask; }

    uint8_t UsedNonVolFRegMask() { return usedNonVolFRegMask; }

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
        RawData rawExTable,
        RawData rawLivenessInfo,
        RawData rawStackPtrsInfo
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
          rawLivenessInfo(rawLivenessInfo),
          rawStackPtrsInfo(rawStackPtrsInfo)
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

    RawData rawExTable       = { IO::FileId(0), 0, 0 };
    RawData rawLivenessInfo  = { IO::FileId(0), 0, 0 };
    RawData rawStackPtrsInfo = { IO::FileId(0), 0, 0 };
};

} // namespace Symlevel
