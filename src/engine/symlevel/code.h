#pragma once

#include "engine/engine.h"
#include "utils/ostream.h"

namespace Symlevel {

struct LivenessInfo {
    uint32_t cbcPos;
    uint16_t regMask;
    std::vector<uint32_t> refSlotNums;
};

struct RawLivenessInfo {
    uint32_t size;
    uint32_t start;
    IO::RandomAccessFile* raf;
};

class Code {
public:
    static Code Parse(Engine::Session& session, IO::FileId fileId, Offset<Code> offset);

    static Code Mock(uint8_t* codePtr, uint32_t codeSize) { return Code(codePtr, codeSize); }

    uint8_t* CodePtr() { return codePtr; }

    uint32_t CodeSize() { return codeSize; }

    uint32_t UntypedSlotCount() { return untypedSlotCount; }

    uint32_t TypedSlotCount() { return typedSlotCount; }

    uint8_t UsedNonVolIRegMask() { return usedNonVolIRegMask; }

    uint8_t UsedNonVolFRegMask() { return usedNonVolFRegMask; }

    std::vector<LivenessInfo> GetLivenessInfo() const;

    friend Stream::Output& operator<<(Stream::Output& out, const Code& code);

private:
    Code(uint8_t* codePtr, uint32_t codeSize) : codePtr(codePtr), codeSize(codeSize) {}

    Code(
        uint32_t untypedSlotCount,
        uint32_t typedSlotCount,
        uint32_t ohmSlotCount,
        uint8_t usedNonVolIRegMask,
        uint8_t usedNonVolFRegMask,
        uint32_t maxCalleeStackArgsCount,
        bool mayHaveNativeCalls,
        bool hasTrivialXHandler,
        uint32_t codeSize,
        uint8_t* codePtr,
        RawLivenessInfo rawLivenessInfo
    )
        : untypedSlotCount(untypedSlotCount),
          typedSlotCount(typedSlotCount),
          ohmSlotCount(ohmSlotCount),
          usedNonVolIRegMask(usedNonVolIRegMask),
          usedNonVolFRegMask(usedNonVolFRegMask),
          maxCalleeStackArgsCount(maxCalleeStackArgsCount),
          mayHaveNativeCalls(mayHaveNativeCalls),
          hasTrivialXHandler(hasTrivialXHandler),
          codePtr(codePtr),
          codeSize(codeSize),
          rawLivenessInfo(rawLivenessInfo)
    {}

    uint32_t untypedSlotCount = 0;
    uint32_t typedSlotCount   = 0;
    uint32_t ohmSlotCount     = 0;

    uint8_t usedNonVolIRegMask       = 0;
    uint8_t usedNonVolFRegMask       = 0;
    uint32_t maxCalleeStackArgsCount = 0;

    bool mayHaveNativeCalls = false;
    bool hasTrivialXHandler = true;

    uint32_t codeSize;
    uint8_t* codePtr;

    RawLivenessInfo rawLivenessInfo = { 0, 0, nullptr };
};

} // namespace Symlevel
