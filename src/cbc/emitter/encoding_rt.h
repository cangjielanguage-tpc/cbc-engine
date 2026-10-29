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
void Encode(ByteBuffer& buf, RT::B3rrrr command);
void Encode(ByteBuffer& buf, RT::InitOption command);
void Encode(ByteBuffer& buf, RT::B4xi12rr command);
void Encode(ByteBuffer& buf, RT::BinaryChecked command);
void Encode(ByteBuffer& buf, RT::BinarySaturating command);
void Encode(ByteBuffer& buf, RT::BFX command);
void Encode(ByteBuffer& buf, RT::IOF command);
void Encode(ByteBuffer& buf, RT::B5xi12ri12 command);
void Encode(ByteBuffer& buf, RT::VirtualCall command);
void Encode(ByteBuffer& buf, RT::B5i32 command);
void Encode(ByteBuffer& buf, RT::B6xri32 command);
void Encode(ByteBuffer& buf, RT::B10xri64 command);
void Encode(ByteBuffer& buf, RT::B9i64 command);
void Encode(ByteBuffer& buf, RT::B7xrrri32 command);
void Encode(ByteBuffer& buf, RT::InterfaceCall command);
void Encode(ByteBuffer& buf, RT::InterfaceCallGeneric command);
void Encode(ByteBuffer& buf, RT::B13i64i32 command);
void Encode(ByteBuffer& buf, RT::StructFieldOp command);
void Encode(ByteBuffer& buf, RT::StoreGenericOp command);
void Encode(ByteBuffer& buf, RT::StoreFrameImm command);
void Encode(ByteBuffer& buf, RT::AtomicOp command);
void Encode(ByteBuffer& buf, RT::StackParam command);
void Encode(ByteBuffer& buf, RT::StackParamF command);
void Encode(ByteBuffer& buf, RT::StackParamS16 command);
void Encode(ByteBuffer& buf, RT::StackParamS32 command);
void Encode(ByteBuffer& buf, RT::LeaStackParam command);

void Encode(ByteBuffer& buf, RT::CopyFieldOp command);
void Encode(ByteBuffer& buf, RT::CopyDerived command);
void Encode(ByteBuffer& buf, RT::CopyDerivedGeneric command);
void Encode(ByteBuffer& buf, RT::CopyLocal command);
void Encode(ByteBuffer& buf, RT::CopyLocalGeneric command);
void Encode(ByteBuffer& buf, RT::CopyTypeSlot command);
void Encode(ByteBuffer& buf, RT::CopyTypeSlotGeneric command);
void Encode(ByteBuffer& buf, RT::CopyToTypeSlot command);
void Encode(ByteBuffer& buf, RT::CopyToTypeSlotGeneric command);
void Encode(ByteBuffer& buf, RT::CopyFromTypeSlot command);
void Encode(ByteBuffer& buf, RT::CopyFromTypeSlotGeneric command);
void Encode(ByteBuffer& buf, RT::Index command);
void Encode(ByteBuffer& buf, RT::IndexGeneric command);
void Encode(ByteBuffer& buf, RT::ConstIndexGeneric command);
} // namespace Emitter
} // namespace Cbc

#endif // CBC_EMITTER_ENCODING_RT
