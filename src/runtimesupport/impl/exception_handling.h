#pragma once

#include "RTInterface.h"
#include "interpreter/function_handle.h"

#include <stdint.h>

namespace EHSupport {

extern "C" uint8_t engine_get_exception_handler(
    Interpretation::DynamicFunctionHandle* handle, Decoder::ByteReader& reader
);

void FrameInfoProvider(DYN_InstructionPointer ip, DYN_FramePointer fp, INT_InterpretedFrameInfo* info);

void FrameDescProvider(INT_FunctionHandle fuh, INT_BytecodePos pos, INT_InterpretedFrameDesc* frameDesc);

} // namespace EHSupport
