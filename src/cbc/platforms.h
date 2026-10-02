#pragma once

#include "cbc/isa.h"
#include "asm_export.h"

namespace Cbc {

enum class Platform {
    LINUX_X64,
    LINUX_AARCH64,
};

template <Platform p>
struct PlatformTraits {};

template <>
struct PlatformTraits<Platform::LINUX_X64> {
    static constexpr IReg TR = IReg::IR7;

    static constexpr IReg IR_HEAD_AREA[] = {
        IReg::IR1, IReg::IR2, IReg::IR3, IReg::IR4,
        IReg::IR5, IReg::IR6,
    };

    static constexpr FReg FR_HEAD_AREA[] = {
        FReg::FR0, FReg::FR1, FReg::FR2, FReg::FR3,
        FReg::FR4, FReg::FR5, FReg::FR6, FReg::FR7
    };

    static constexpr int IR_PARAM_COUNT = sizeof(IR_HEAD_AREA) / sizeof(IR_HEAD_AREA[0]);
    static constexpr int FR_PARAM_COUNT = sizeof(FR_HEAD_AREA) / sizeof(FR_HEAD_AREA[0]);

    static constexpr bool SRET_SHIFTS = true;
};

template <>
struct PlatformTraits<Platform::LINUX_AARCH64> {
    static constexpr IReg TR = IReg::IR10;

    static constexpr IReg IR_HEAD_AREA[] = {
        IReg::IR1, IReg::IR2, IReg::IR3, IReg::IR4,
        IReg::IR5, IReg::IR6, IReg::IR7, IReg::IR8,
    };

    static constexpr FReg FR_HEAD_AREA[] = {
        FReg::FR0, FReg::FR1, FReg::FR2, FReg::FR3,
        FReg::FR4, FReg::FR5, FReg::FR6, FReg::FR7
    };

    static constexpr int IR_PARAM_COUNT = sizeof(IR_HEAD_AREA) / sizeof(IR_HEAD_AREA[0]);
    static constexpr int FR_PARAM_COUNT = sizeof(FR_HEAD_AREA) / sizeof(FR_HEAD_AREA[0]);

    static constexpr bool SRET_SHIFTS = true;
    static constexpr IReg SRET_REG = IReg::IR9;
};

#if defined(__x86_64__) || defined(_M_X64)
static constexpr Platform HOST_PLATFORM = Platform::LINUX_X64;
#elif defined(__aarch64__) || defined(_M_ARM64)
static constexpr Platform HOST_PLATFORM = Platform::LINUX_AARCH64;
#endif

using HostTraits = PlatformTraits<HOST_PLATFORM>;

static_assert(IReg::TAIL_REG == HostTraits::TR);
}
