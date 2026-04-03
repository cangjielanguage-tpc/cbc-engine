#pragma once

#include <memory>

#include "isa_parser.h"

namespace Cbc {

void EnableRawDisasm();
bool IsRawDisasmEnabled();
std::unique_ptr<IsaParser> RawDisasm(std::ostream& stream, Cbc::MethodCode code);
std::unique_ptr<IsaParser> RawDisasm(std::ostream& stream, Decoder::FatByteReader reader);
std::unique_ptr<IsaParser> RawDisasm(std::ostream& stream, uint8_t* start, uint8_t* end);

} // namespace Cbc
