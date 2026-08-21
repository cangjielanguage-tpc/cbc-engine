#pragma once

#include "isa_parser.h"
#include "resolution/resolution.h"
#include "utils/ostream.h"

namespace Cbc {

extern bool g_IsRawDisasmEnabled;

void EnableRawDisasm();

void RawDisasm(Stream::Output& stream, Cbc::MethodCode code);
void RawDisasm(Stream::Output& stream, Decoder::FatByteReader reader);
void RawDisasm(Stream::Output& stream, uint8_t* start, uint8_t* end);
void Disasm(Stream::Output& stream, Cbc::MethodCode code, Resolution::Resolver* resolver);
void Disasm(Stream::Output& stream, Decoder::FatByteReader reader, Resolution::Resolver* resolver);
void Disasm(Stream::Output& stream, uint8_t* start, uint8_t* end, Resolution::Resolver* resolver);

void DisasmOnce(Stream::Output& stream, Decoder::FatByteReader reader, Resolution::Resolver* resolver);

} // namespace Cbc
