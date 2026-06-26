#pragma once

#include "interpreter/function_handle.h"
#include <stdint.h>

extern "C" uint8_t engine_get_exception_handler(
    Interpretation::DynamicFunctionHandle* handle, Decoder::FatByteReader& reader
);
