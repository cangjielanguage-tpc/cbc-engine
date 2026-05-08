#include "cbc/isa_rt.h"
#include "segment.h"

namespace Cbc {
namespace Emitter {

void Encode(ByteBuffer& buf, RT::Opcode opc) { buf.AddW8(opc); }

void Encode(ByteBuffer& buf, Format::RR rr) { buf.AddW8(static_cast<uint32_t>(rr.x | (rr.y << 4))); }

void Encode(ByteBuffer& buf, Format::XR xr) { buf.AddW8(static_cast<uint32_t>(xr.imm | (xr.r << 4))); }

void Encode(ByteBuffer& buf, Format::XX xx) { buf.AddW8(static_cast<uint32_t>(xx.imm1 | (xx.imm2 << 4))); }

void Encode(ByteBuffer& buf, Format::Imm16 i16) { buf.AddW16(i16.imm); }

void Encode(ByteBuffer& buf, Format::XImm12 xi12) { buf.AddW16(Format::XImm12::Raw(xi12)); }

void Encode(ByteBuffer& buf, RT::B1 command) { Encode(buf, command.opc); }

void Encode(ByteBuffer& buf, RT::B2rr command)
{
    Encode(buf, command.opc);
    Encode(buf, command.rr);
}

void Encode(ByteBuffer& buf, RT::B2xr command)
{
    Encode(buf, command.opc);
    Encode(buf, command.xr);
}

void Encode(ByteBuffer& buf, RT::B3xrrr command)
{
    Encode(buf, command.opc);
    Encode(buf, command.xr);
    Encode(buf, command.rr);
}

void Encode(ByteBuffer& buf, RT::B3xxrr command)
{
    Encode(buf, command.opc);
    Encode(buf, command.xx);
    Encode(buf, command.rr);
}

void Encode(ByteBuffer& buf, RT::B4xi12rr command)
{
    Encode(buf, command.opc);
    Encode(buf, command.xi12);
    Encode(buf, command.rr);
}

void Encode(ByteBuffer& buf, RT::BFX command)
{
    Encode(buf, command.opc);
    Encode(buf, command.rr);
    buf.AddW8(command.offs);
    buf.AddW8(command.size);
}

void Encode(ByteBuffer& buf, RT::MemOpcode opc) { buf.AddW8(opc); }

void Encode(ByteBuffer& buf, RT::M2i8 command)
{
    Encode(buf, command.opc);
    buf.AddW8(command.imm8);
}

void Encode(ByteBuffer& buf, RT::M3i16 command)
{
    Encode(buf, command.opc);
    buf.AddW16(command.imm16);
}

void Encode(ByteBuffer& buf, RT::M5i32 command)
{
    Encode(buf, command.opc);
    buf.AddW32(command.imm32);
}

void Encode(ByteBuffer& buf, RT::M9i64 command)
{
    Encode(buf, command.opc);
    buf.AddW64(command.imm64);
}

void Encode(ByteBuffer& buf, RT::M2rr command)
{
    Encode(buf, command.opc);
    Encode(buf, command.rr);
}

void Encode(ByteBuffer& buf, RT::M2xr command)
{
    Encode(buf, command.opc);
    Encode(buf, command.xr);
}

void Encode(ByteBuffer& buf, Format::Imm32 i32) { buf.AddW32(i32.imm); }

void Encode(ByteBuffer& buf, Format::Imm64 i64) { buf.AddW64(i64.imm); }

void Encode(ByteBuffer& buf, Format::RImm12 ri12) { buf.AddW16(Format::RImm12::Raw(ri12)); }

void Encode(ByteBuffer& buf, RT::B5xi12ri12 command)
{
    Encode(buf, command.opc);
    Encode(buf, command.xi12);
    Encode(buf, command.ri12);
}

void Encode(ByteBuffer& buf, RT::B5i16i16 command)
{
    Encode(buf, command.opc);
    Encode(buf, command.imm1);
    Encode(buf, command.imm2);
}

void Encode(ByteBuffer& buf, RT::B5i32 command)
{
    Encode(buf, command.opc);
    Encode(buf, command.imm32);
}

void Encode(ByteBuffer& buf, RT::B6xri32 command)
{
    Encode(buf, command.opc);
    Encode(buf, command.xr);
    Encode(buf, command.imm32);
}

void Encode(ByteBuffer& buf, RT::B10xri64 command)
{
    Encode(buf, command.opc);
    Encode(buf, command.xr);
    Encode(buf, command.imm64);
}

void Encode(ByteBuffer& buf, RT::B9i64 command)
{
    Encode(buf, command.opc);
    Encode(buf, command.imm64);
}

void Encode(ByteBuffer& buf, RT::B11i16i64 command)
{
    Encode(buf, command.opc);
    Encode(buf, command.imm16);
    buf.AddW64(command.imm64.imm);
}

} // namespace Emitter
} // namespace Cbc
