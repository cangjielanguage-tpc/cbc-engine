#pragma once

#include "api/resolver.h"
#include "isa_parser.h"
#include "utils/ostream.h"

namespace Cbc {

void EnableRawDisasm();
void EnableDisasm();
bool IsRawDisasmEnabled();
bool IsDisasmEnabled();

void RawDisasm(Stream::Output& stream, Cbc::MethodCode code);
void RawDisasm(Stream::Output& stream, Decoder::FatByteReader reader);
void RawDisasm(Stream::Output& stream, uint8_t* start, uint8_t* end);
void Disasm(Stream::Output& stream, Cbc::MethodCode code, API::Resolver* resolver);
void Disasm(Stream::Output& stream, Decoder::FatByteReader reader, API::Resolver* resolver);
void Disasm(Stream::Output& stream, uint8_t* start, uint8_t* end, API::Resolver* resolver);

} // namespace Cbc
