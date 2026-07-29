#pragma once

#if defined(__x86_64__) || defined(_M_X64)

    // On x64 receiver location does depend on sret,
    // because `sret` uses first register in calling convention
    #define HAS_SRET_SHIFT 1

#elif defined(__aarch64__) || defined(_M_ARM64)

    // On aarch64 receiver location does not depend on sret,
    // because sret has dedicated register IR9.
    #define HAS_SRET_SHIFT 0

#else
    #error "unsupported platform"
#endif
