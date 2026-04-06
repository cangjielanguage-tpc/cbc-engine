#pragma once

#include <memory>

#include "isa_parser.h"
#include "utils/ostream.h"

namespace Cbc {

void EnableRawDisasm();
bool IsRawDisasmEnabled();
std::unique_ptr<IsaParser> RawDisasm(Stream::Out stream, Cbc::MethodCode code);
std::unique_ptr<IsaParser> RawDisasm(Stream::Out stream, Decoder::FatByteReader reader);
std::unique_ptr<IsaParser> RawDisasm(Stream::Out stream, uint8_t* start, uint8_t* end);

} // namespace Cbc
