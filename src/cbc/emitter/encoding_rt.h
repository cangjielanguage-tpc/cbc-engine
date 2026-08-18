#ifndef CBC_EMITTER_ENCODING_RT
#define CBC_EMITTER_ENCODING_RT

#include "cbc/isa_rt.h"
#include "segment.h"

namespace Cbc {
namespace Emitter {

void Encode(ByteBuffer& buf, Format::RR rr);
void Encode(ByteBuffer& buf, Format::XR xr);
void Encode(ByteBuffer& buf, Format::XX xx);
void Encode(ByteBuffer& buf, Format::XImm12 xi12);
void Encode(ByteBuffer& buf, Format::Imm8 i8);
void Encode(ByteBuffer& buf, Format::Imm16 i16);
void Encode(ByteBuffer& buf, Format::Imm32 i32);
void Encode(ByteBuffer& buf, Format::Imm64 i64);
void Encode(ByteBuffer& buf, Format::RImm12 ri12);
void Encode(ByteBuffer& buf, RT::Opcode opc);
void Encode(ByteBuffer& buf, RT::B1 command);
void Encode(ByteBuffer& buf, RT::B2rr command);
void Encode(ByteBuffer& buf, RT::B2xr command);
void Encode(ByteBuffer& buf, RT::B3xri8 command);
void Encode(ByteBuffer& buf, RT::B4xri16 command);
void Encode(ByteBuffer& buf, RT::B3xrrr command);
void Encode(ByteBuffer& buf, RT::B3xxrr command);
void Encode(ByteBuffer& buf, RT::B3xi12 command);
void Encode(ByteBuffer& buf, RT::B4xi12rr command);
void Encode(ByteBuffer& buf, RT::BinaryChecked command);
void Encode(ByteBuffer& buf, RT::BFX command);
void Encode(ByteBuffer& buf, RT::IOF command);
void Encode(ByteBuffer& buf, RT::B5xi12ri12 command);
void Encode(ByteBuffer& buf, RT::VirtualCall command);
void Encode(ByteBuffer& buf, RT::B5i32 command);
void Encode(ByteBuffer& buf, RT::B6xri32 command);
void Encode(ByteBuffer& buf, RT::B10xri64 command);
void Encode(ByteBuffer& buf, RT::B9i64 command);
void Encode(ByteBuffer& buf, RT::InterfaceCall command);
void Encode(ByteBuffer& buf, RT::InterfaceCallGeneric command);
void Encode(ByteBuffer& buf, RT::B13i64i32 command);
void Encode(ByteBuffer& buf, RT::StructFieldOp command);
void Encode(ByteBuffer& buf, RT::AtomicOp command);

void Encode(ByteBuffer& buf, RT::MemOpcode opc);
void Encode(ByteBuffer& buf, RT::M2i8 command);
void Encode(ByteBuffer& buf, RT::M3i16 command);
void Encode(ByteBuffer& buf, RT::M5i32 command);
void Encode(ByteBuffer& buf, RT::M9i64 command);
void Encode(ByteBuffer& buf, RT::M3xri8 command);
void Encode(ByteBuffer& buf, RT::M4xri16 command);
void Encode(ByteBuffer& buf, RT::M6xri32 command);
void Encode(ByteBuffer& buf, RT::M10xri64 command);
void Encode(ByteBuffer& buf, RT::M2rr command);
void Encode(ByteBuffer& buf, RT::M2xr command);
void Encode(ByteBuffer& buf, RT::M3xrrr command);
void Encode(ByteBuffer& buf, RT::M3rrrr command);
void Encode(ByteBuffer& buf, RT::M3rri8 command);
void Encode(ByteBuffer& buf, RT::M4rri16 command);
void Encode(ByteBuffer& buf, RT::M6rri32 command);
void Encode(ByteBuffer& buf, RT::M10rri64 command);
void Encode(ByteBuffer& buf, RT::MStructFieldOp command);
void Encode(ByteBuffer& buf, RT::CopyDerived command);

} // namespace Emitter
} // namespace Cbc

#endif // CBC_EMITTER_ENCODING_RT
