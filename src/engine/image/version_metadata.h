#pragma once

#include "utils/inclusive_range.h"

namespace Image {

struct VersionMetadata {
    static constexpr InclusiveRange SUPPORTED_FILE_VESRION { 1, 1 };
    static constexpr InclusiveRange SUPPORTED_BYTECODE_VERSION { 1, 1 };

    const uint8_t fileVersion;
    const uint8_t bytecodeVersion;

    VersionMetadata(uint8_t fileVersion, uint8_t bytecodeVersion)
        : fileVersion(fileVersion),
          bytecodeVersion(bytecodeVersion)
    {
        // TODO: throw proper exception
        ASSERTION(SUPPORTED_FILE_VESRION.Covers(fileVersion), "unsupported file version");
        ASSERTION(SUPPORTED_FILE_VESRION.Covers(bytecodeVersion), "unsupported bytecode version");
    }
};

} // namespace Image
