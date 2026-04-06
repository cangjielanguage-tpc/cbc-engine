#pragma once
#include "interpreter/code.h"
#include "interpreter/literals.h"
#include "isa_rt.h"
#include "utils/ostream.h"

namespace Cbc {
namespace RT {

void Log(Interpretation::Code code, const Stream::Out& stream);

void Log(Interpretation::LiteralTable* table, const Stream::Out& stream, B1 args);
void Log(Interpretation::LiteralTable* table, const Stream::Out& stream, B2rr args);
void Log(Interpretation::LiteralTable* table, const Stream::Out& stream, B2xr args);
void Log(Interpretation::LiteralTable* table, const Stream::Out& stream, B6xri32 args);
void Log(Interpretation::LiteralTable* table, const Stream::Out& stream, B10xri64 args);
void Log(Interpretation::LiteralTable* table, const Stream::Out& stream, B4xi12rr args);
void Log(Interpretation::LiteralTable* table, const Stream::Out& stream, B5xi12ri12 args);
void Log(Interpretation::LiteralTable* table, const Stream::Out& stream, B5i32 args);
void Log(Interpretation::LiteralTable* table, const Stream::Out& stream, B3xrrr args);
void Log(Interpretation::LiteralTable* table, const Stream::Out& stream, B4xi12xr args);
void Log(Interpretation::LiteralTable* table, const Stream::Out& stream, B3xi12 args);

} // namespace RT
} // namespace Cbc
