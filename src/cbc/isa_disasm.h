#pragma once

#include <memory>
#include <optional>

#include "api/resolver.h"
#include "isa_parser.h"
#include "utils/ostream.h"

namespace Cbc {

void EnableRawDisasm();
void EnableDisasm();
bool IsRawDisasmEnabled();
bool IsDisasmEnabled();

std::unique_ptr<IsaParser> RawDisasm(Stream::Out& stream, Cbc::MethodCode code);
std::unique_ptr<IsaParser> RawDisasm(Stream::Out& stream, Decoder::FatByteReader reader);
std::unique_ptr<IsaParser> RawDisasm(Stream::Out& stream, uint8_t* start, uint8_t* end);
std::unique_ptr<IsaParser> Disasm(Stream::Out& stream, Cbc::MethodCode code, API::Resolver* resolver);
std::unique_ptr<IsaParser> Disasm(Stream::Out& stream, Decoder::FatByteReader reader, API::Resolver* resolver);
std::unique_ptr<IsaParser> Disasm(Stream::Out& stream, uint8_t* start, uint8_t* end, API::Resolver* resolver);

} // namespace Cbc
