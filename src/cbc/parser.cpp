#include "cbc/parser.h"
#include "cbc/decoder.h"
#include "cbc/isa.h"

namespace Cbc {

static uint8_t* GetCodeEnd(MethodCode code) { return code.CodePtr() + code.CodeSize(); }

static Decoder::ByteReader ReaderOf(MethodCode code)
{
    return Decoder::ByteReader(code.CodePtr(), code.CodePtr(), GetCodeEnd(code));
}

Parser::Parser(API::Resolver* resolver, MethodCode code)
    : resolver(resolver),
      immPrefix(InputImmPrefix::__LAST),
      codeReader(ReaderOf(code)),
      codeEnd(GetCodeEnd(code))
{}

void Parser::Interpret()
{
    while (!codeReader.EndOfMem(codeEnd)) {
        BeforeInterpretOne(codeReader.Cursor());
        InterpretOne(codeReader.Read8());
    }
}

// #include "isa_opcode_def.h"
// GEN_JUMP_TABLE(DO_WITH_ALL_OPCODES(GEN_JUMP_TABLE_ENTRY))
// #include "isa_opcode_undef.h"

inline void ::Cbc::Parser::InterpretOne(uint32_t opcode)
{
    switch (opcode) {
    case ::Cbc::Opc(::Cbc::InputOpcode::Mov32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Mov32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Mov64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Mov64>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Mov32i): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Mov32i>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Mov64i): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Mov64i>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::MovVst): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::MovVst>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::MovRef): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::MovRef>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::ExtendSigned): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::ExtendSigned>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::ExtendUnsigned): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::ExtendUnsigned>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Add32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Add32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Sub32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Sub32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Mul32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Mul32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::And32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::And32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Or32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Or32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Xor32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Xor32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::DivSigned32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::DivSigned32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::RemSigned32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::RemSigned32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::DivUnsigned32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::DivUnsigned32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::RemUnsigned32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::RemUnsigned32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Lsr32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Lsr32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Asr32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Asr32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Lsl32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Lsl32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Add64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Add64>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Sub64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Sub64>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Mul64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Mul64>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::And64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::And64>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Or64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Or64>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Xor64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Xor64>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::DivSigned64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::DivSigned64>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::RemSigned64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::RemSigned64>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::DivUnsigned64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::DivUnsigned64>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::RemUnsigned64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::RemUnsigned64>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Lsr64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Lsr64>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Asr64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Asr64>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Lsl64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Lsl64>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Add32Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Add32Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Sub32Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Sub32Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Mul32Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Mul32Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::And32Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::And32Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Or32Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Or32Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Xor32Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Xor32Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::DivSigned32Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::DivSigned32Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::RemSigned32Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::RemSigned32Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::DivUnsigned32Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::DivUnsigned32Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::RemUnsigned32Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::RemUnsigned32Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Lsr32Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Lsr32Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Asr32Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Asr32Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Lsl32Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Lsl32Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Add64Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Add64Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Sub64Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Sub64Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Mul64Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Mul64Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::And64Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::And64Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Or64Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Or64Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Xor64Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Xor64Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::DivSigned64Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::DivSigned64Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::RemSigned64Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::RemSigned64Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::DivUnsigned64Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::DivUnsigned64Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::RemUnsigned64Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::RemUnsigned64Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Lsr64Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Lsr64Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Asr64Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Asr64Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Lsl64Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Lsl64Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Neg32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Neg32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Neg64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Neg64>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Neg32Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Neg32Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Neg64Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Neg64Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::IntegerCommon32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::IntegerCommon32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::IntegerCommon64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::IntegerCommon64>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::IntegerCommon32K16): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::IntegerCommon32K16>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::IntegerCommon64K16): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::IntegerCommon64K16>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::CheckedAdd): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::CheckedAdd>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::CheckedSub): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::CheckedSub>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::CheckedMul): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::CheckedMul>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::CheckedDiv): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::CheckedDiv>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::CheckedAddImm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::CheckedAddImm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::CheckedSubImm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::CheckedSubImm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::CheckedMulImm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::CheckedMulImm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::CheckedDivImm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::CheckedDivImm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Bfx): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Bfx>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::FloatCommon): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::FloatCommon>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::FloatCommonImm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::FloatCommonImm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::FloatIntegerConversions): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::FloatIntegerConversions>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::FloatFloatConversions): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::FloatFloatConversions>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Bcc): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Bcc>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::BccImm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::BccImm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Jump32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Jump32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::CallDirect): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::CallDirect>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Ret32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Ret32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Ret64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Ret64>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Ret32F): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Ret32F>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Ret64F): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Ret64F>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::ImmPrefix32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::ImmPrefix32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::ImmPrefix64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::ImmPrefix64>(codeReader); break; }
    default: ASSERTION(false, "Unexpected opcode"); break;
    }
}

} // namespace Cbc
