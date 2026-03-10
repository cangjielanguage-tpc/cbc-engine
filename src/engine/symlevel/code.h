#pragma once

#include "engine/engine.h"

namespace Symlevel {

class Code {
public:
    static Code Parse(Engine::Session& session, IO::FileId fileId, Offset<Code> offset);

    static Code Mock(uint8_t* codePtr, uint32_t codeSize) { return Code(codePtr, codeSize); }

    uint8_t* CodePtr() { return codePtr; }

    uint32_t CodeSize() { return codeSize; }

private:
    Code(uint8_t* codePtr, uint32_t codeSize) : codePtr(codePtr), codeSize(codeSize) {}

    Code(Engine::Session& session, IO::StreamFileReader& reader);

    uint32_t untypedSlotCount;
    uint32_t typedSlotCount;
    uint32_t ohmSlotCount;

    uint8_t usedNonVolIRegMask;
    uint8_t usedNonVolFRegMask;
    uint32_t maxCalleeStackArgsCount;

    bool mayHaveNativeCalls;
    bool hasTrivialXHandler;

    uint32_t codeSize;
    uint8_t* codePtr;

    uint32_t xInfoSize;
    uint8_t* xInfoPtr;
};

} // namespace Symlevel
