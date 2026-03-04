#pragma once
#include <ostream>

#include "interpreter/code.h"
#include "interpreter/literals.h"
#include "isa_rt.h"

namespace Cbc {
namespace RT {

void Log(Interpretation::Code code, std::ostream& stream);

void Log(Interpretation::LiteralTable* table, std::ostream& stream, B1 args);
void Log(Interpretation::LiteralTable* table, std::ostream& stream, B2rr args);
void Log(Interpretation::LiteralTable* table, std::ostream& stream, B2xr args);
void Log(Interpretation::LiteralTable* table, std::ostream& stream, B6xri32 args);
void Log(Interpretation::LiteralTable* table, std::ostream& stream, B10xri64 args);
void Log(Interpretation::LiteralTable* table, std::ostream& stream, B4xi12rr args);
void Log(Interpretation::LiteralTable* table, std::ostream& stream, B5xi12ri12 args);
void Log(Interpretation::LiteralTable* table, std::ostream& stream, B5i32 args);
void Log(Interpretation::LiteralTable* table, std::ostream& stream, B3xrrr args);
void Log(Interpretation::LiteralTable* table, std::ostream& stream, B4xi12xr args);
void Log(Interpretation::LiteralTable* table, std::ostream& stream, B3xi12 args);

} // namespace RT
} // namespace Cbc
