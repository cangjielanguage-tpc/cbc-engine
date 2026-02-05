#ifndef CBC_EMITTER_ENCODING_RT
#define CBC_EMITTER_ENCODING_RT

#include "segment.h"
#include "cbc/isa_rt.h"

namespace Cbc {
namespace Emitter {

void Encode(ByteBuffer& buf, RT::Opcode opc);
void Encode(ByteBuffer& buf, RT::RR rr);
void Encode(ByteBuffer& buf, RT::XR xr);
void Encode(ByteBuffer& buf, RT::Imm16 i16);
void Encode(ByteBuffer& buf, RT::XImm12 xi12);
void Encode(ByteBuffer& buf, RT::B1 command);
void Encode(ByteBuffer& buf, RT::B2rr command);
void Encode(ByteBuffer& buf, RT::B2xr command);
void Encode(ByteBuffer& buf, RT::B3xrrr command);
void Encode(ByteBuffer& buf, RT::B4xi12rr command);

void Encode(ByteBuffer& buf, RT::MemOpcode opc);
void Encode(ByteBuffer& buf, RT::M2i16 command);
void Encode(ByteBuffer& buf, RT::M2i32 command);
void Encode(ByteBuffer& buf, RT::M2i64 command);
void Encode(ByteBuffer& buf, RT::M2rr command);
void Encode(ByteBuffer& buf, RT::M2xr command);

} // namespace Emitter
} // namespace Cbc

#endif // CBC_EMITTER_ENCODING_RT
