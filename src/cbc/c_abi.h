#pragma once

#include "isa.h"
#include "utils/span.h"
#include "utils/vector.h"
#include <cstdint>

namespace Engine {
struct Term;
}

namespace Resolution {
struct Resolver;
}

namespace Cbc {

enum class CAbi {
    SYSV,
    AAPCS64,
};

// Leaf field info of a struct.
struct CAbiField {
    uint32_t offset;
    uint8_t size;
    bool isFloat;
};

// Register-resident chunk of a by-value struct.
struct CAbiPiece {
    uint32_t offset;
    uint8_t size;
    bool isFloat;

    using LDK = Format::LoadAccessKind;

    LDK Ldk() const
    {
        if (isFloat) {
            switch (size) {
                case 8:  return LDK::LD_F64;
                default: return LDK::LD_F32;
            }
        } else {
            switch (size) {
                case 8:  return LDK::LD_64;
                case 4:  return LDK::LD_32;
                case 2:  return LDK::LD_U16;
                default: return LDK::LD_U8;
            }
        }
    }
};

enum class CAbiPassBy {
    EMPTY, // for zero-sized structs
    REGS,
    STACK,
};

struct CAbiStructPlan {
    CAbiPassBy passBy;
    Utils::Vector<CAbiPiece> pieces; // filled only when passBy == REGS
};

// Classifies placement of by-value struct by its flattened fields information and other parameters.
// `fields` must be sorted by offset and cover every non-padding byte group.
CAbiStructPlan ClassifyCAbiStruct(
    Utils::Span<const CAbiField> fields, uint32_t size, bool isVarArg, int freeIRegs, int freeFRegs
);

bool FlattenCAbiFields(
    Resolution::Resolver& resolver, Engine::Term term, uint32_t base, Utils::Vector<CAbiField>& fields
);

} // namespace Cbc
