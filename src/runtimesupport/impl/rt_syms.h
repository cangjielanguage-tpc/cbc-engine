#pragma once

#include "RTInterface.h"
#include "RuntimeTypes.h"

namespace RTSupport {

void Initialize(DYN_CJNativeInterface* interf);

void* GetSymbolAddr(const char* libName, const char* symName);

} // namespace RTSupport
