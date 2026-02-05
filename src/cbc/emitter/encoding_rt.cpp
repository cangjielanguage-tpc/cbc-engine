#include "segment.h"
#include "cbc/isa_rt.h"

namespace Cbc {
namespace Emitter {

void Encode(ByteBuffer& buf, RT::Opcode opc) {
    buf.AddW8(opc);
}

void Encode(ByteBuffer& buf, RT::RR rr) {
    buf.AddW8(static_cast<uint32_t>(rr.x | (rr.y << 4)));
}

void Encode(ByteBuffer& buf, RT::XR xr) {
    buf.AddW8(static_cast<uint32_t>(xr.imm | (xr.r << 4)));
}

void Encode(ByteBuffer& buf, RT::Imm16 i16) {
    buf.AddW16(i16.imm);
}

void Encode(ByteBuffer& buf, RT::XImm12 xi12) {
    buf.AddW16(RT::XImm12::Raw(xi12));
}

void Encode(ByteBuffer& buf, RT::B1 command) {
    Encode(buf, command.opc);
}

void Encode(ByteBuffer& buf, RT::B2rr command) {
    Encode(buf, command.opc);
    Encode(buf, command.rr);
}

void Encode(ByteBuffer& buf, RT::B2xr command) {
    Encode(buf, command.opc);
    Encode(buf, command.xr);
}

void Encode(ByteBuffer& buf, RT::B3xrrr command) {
    Encode(buf, command.opc);
    Encode(buf, command.xr);
    Encode(buf, command.rr);
}

void Encode(ByteBuffer& buf, RT::B4xi12rr command) {
    Encode(buf, command.opc);
    Encode(buf, command.xi12);
    Encode(buf, command.rr);
}

void Encode(ByteBuffer& buf, RT::MemOpcode opc) {
    buf.AddW8(opc);
}

void Encode(ByteBuffer& buf, RT::M2i16 command) {
    Encode(buf, command.opc);
    buf.AddW16(command.imm16);
}

void Encode(ByteBuffer& buf, RT::M2i32 command) {
    Encode(buf, command.opc);
    buf.AddW32(command.imm32);
}

void Encode(ByteBuffer& buf, RT::M2i64 command) {
    Encode(buf, command.opc);
    buf.AddW64(command.imm64);
}

void Encode(ByteBuffer& buf, RT::M2rr command) {
    Encode(buf, command.opc);
    Encode(buf, command.rr);
}

void Encode(ByteBuffer& buf, RT::M2xr command) {
    Encode(buf, command.opc);
    Encode(buf, command.xr);
}

} // namespace Emitter
} // namespace Cbc
