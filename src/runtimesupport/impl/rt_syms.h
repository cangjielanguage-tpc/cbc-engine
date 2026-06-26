#pragma once

#include "RTInterface.h"
#include "RuntimeTypes.h"

namespace RTSupport {

void Initialize(DYN_CJNativeInterface* interf);

extern void (*WriteStructField)(
    uintptr_t base, uintptr_t field, size_t fieldLen, uintptr_t src, size_t srcLen, DYN_GCTib gctib
);
extern void (*ReadStructField)(uintptr_t dst, uintptr_t base, uintptr_t field, size_t fieldLen, DYN_GCTib gctib);

} // namespace RTSupport
