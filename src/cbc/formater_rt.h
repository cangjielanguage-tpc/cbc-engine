#pragma once
#include "interpreter/code.h"
#include "interpreter/literals.h"
#include "isa_rt.h"
#include "utils/ostream.h"

namespace Cbc {
namespace RT {

void Log(Interpretation::Code code, Stream::Output& stream);

void Log(Interpretation::LiteralTable* table, Stream::Output& stream, B1 args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, B2rr args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, B2xr args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, B3xri8 args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, B4xri16 args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, B6xri32 args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, B10xri64 args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, B4xi12rr args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, BFX args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, IOF args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, B5xi12ri12 args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, VirtualCall args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, B5i32 args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, B3xrrr args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, B3xxrr args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, B4xi12xr args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, B3xi12 args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, B3rrrr args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, B9i64 args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, B7xrrri32 args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, BinaryChecked args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, InterfaceCall args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, InterfaceCallGeneric args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, B13i64i32 args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, StructFieldOp args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, AtomicOp args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, Offset args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, M1 args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, M2rr args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, M2xr args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, M2i8 args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, M3xrrr args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, M3rrrr args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, M3i16 args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, M5i32 args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, M9i64 args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, M3xri8 args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, M4xri16 args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, M6xri32 args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, M10xri64 args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, M3rri8 args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, M4rri16 args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, M6rri32 args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, M10rri64 args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, MStructFieldOp args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, CopyFieldOp args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, CopyDerived args);
void Log(Interpretation::LiteralTable* table, Stream::Output& stream, Index args);
} // namespace RT
} // namespace Cbc
