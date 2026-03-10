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

    uint32_t xInfoSize = 0;
    uint8_t* xInfoPtr  = nullptr;
};

} // namespace Symlevel
