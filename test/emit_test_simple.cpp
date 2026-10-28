#include "emit_test_common.h"

namespace Cbc {
namespace Emitter {

struct Test;

using namespace Cbc::Format;

// ---------------------------------------------------------------------------
// Closures, objects and boxes
// ---------------------------------------------------------------------------

TEST(EmitTest, InitClosureVariants)
{
    Emitter e;
    e.InitClosure(false);
    e.InitClosure(true);

    auto code = e.Build(heap);
    EXPECT_EQ(static_cast<uint8_t>(RT::Opcode::INITCLOSURE), code.bytecode[0]);
    EXPECT_EQ(static_cast<uint8_t>(RT::Opcode::INITCLOSURE_SRET), code.bytecode[1]);

    CHECK_DISASM(
        code,
        "0x000: init.closure",
        "0x001: init.closure.sret"
    );
}

TEST(EmitTest, NewObjGenericVariants)
{
    Emitter e;
    e.NewObjGeneric(IReg::IR2);
    e.NewObjGeneric(IReg::IR3, true);

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: newobj.g IR2",
        "0x002: newobj.pinned.g IR3"
    );
}

TEST(EmitTest, NewBoxBuiltinType)
{
    Emitter e;
    e.NewBox(Interpretation::BUILTIN_RUNE);

    auto code = e.Build(heap);
    CHECK_DISASM(code, "0x000: newbox 0xf");
}

// ---------------------------------------------------------------------------
// Simple arithmetic and moves
// ---------------------------------------------------------------------------

TEST(EmitTest, Simple_Mov)
{
    Emitter e;
    e.MovImm(Width::W64, IReg::IR2, 0x7);
    e.Mov(IReg::IR1, IReg::IR2);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: movi IR2 7",
        "0x002: mov IR1 IR2",
        "0x004: ret"
    );
    auto res = Interpret(code, U32(0), U32(0));
    EXPECT_EQ(res.u64, 0x7);
}

TEST(EmitTest, Simple_MovF2I)
{
    Emitter e;
    e.FMovI32(FReg::FR0, 12345.678);
    e.Mov(IReg::IR1, FReg::FR0);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: fmovi.32 FR0 12345.677734",
        "0x006: f2i IR1 FR0",
        "0x008: ret"
    );
    auto res = Interpret(code, U32(0), U32(0));
    EXPECT_EQ(res.u32, 0x4640e6b6);
}

TEST(EmitTest, Simple_FMovI32)
{
    Emitter e;
    e.FMovI32(FReg::FR0, 0.5);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: fmovi.32 FR0 0.500000",
        "0x006: ret"
    );
    auto res = InterpretFPRes(code, F32(0), F32(0));
    EXPECT_EQ(res.f32, 0.5);
}

TEST(EmitTest, Simple_FMovI64)
{
    Emitter e;
    e.FMovI64(FReg::FR10, 0.25);
    e.Mov(FReg::FR0, FReg::FR10);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: fmovi.64 FR10 0.250000",
        "0x00a: fmov FR0 FR10",
        "0x00c: ret"
    );
    auto res = InterpretFPRes(code, F32(0), F32(0));
    EXPECT_EQ(res.f64, 0.25);
}

TEST(EmitTest, Simple_ArithB2rr)
{
    Emitter e;
    e.Add(Width::W32, IReg::IR1, IReg::IR1, IReg::IR2);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: add.32 IR1 IR1 IR2",
        "0x003: ret"
    );
    auto res = Interpret(code, U32(1), U32(2));
    EXPECT_EQ(res.u32, 3);
}

TEST(EmitTest, Simple_ArithB3xrrr)
{
    Emitter e;
    e.Add(Width::W32, IReg::IR1, IReg::IR2, IReg::IR1);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: add.32 IR1 IR2 IR1",
        "0x003: ret"
    );
    auto res = Interpret(code, U32(1), U32(2));
    EXPECT_EQ(res.u32, 3);
}

TEST(EmitTest, Simple_ArithB4xi12rr)
{
    Emitter e;
    e.AddI(Width::W32, IReg::IR1, IReg::IR2, 0xff);
    e.AddI(Width::W32, IReg::IR1, IReg::IR1, 0xff00); // through literal
    e.BinaryImm(Common::SUB, Width::W32, IReg::IR1, IReg::IR1, 0xfff);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: add.32 IR1 IR2 255",
        "0x004: add.32 IR1 IR1 65280",
        "0x008: sub.32 IR1 IR1 4095",
        "0x00c: ret"
    );
    auto res = Interpret(code, U32(0), U32(1));
    EXPECT_EQ(res.u32, 0xf001);
}

TEST(EmitTest, Simple_ArithFP)
{
    Emitter e;
    e.FMovI32(FReg::FR0, 12.5);
    e.FMovI32(FReg::FR1, 2.5);
    e.FMovI32(FReg::FR2, 1.25);
    e.FMovI32(FReg::FR3, 0.25);
    e.Binary(FloatOperations::FADD, Width::W32, FReg::FR0, FReg::FR0, FReg::FR1);
    e.Binary(FloatOperations::FSUB, Width::W32, FReg::FR0, FReg::FR0, FReg::FR2);
    e.Binary(FloatOperations::FDIV, Width::W32, FReg::FR0, FReg::FR0, FReg::FR2);
    e.Binary(FloatOperations::FMUL, Width::W32, FReg::FR0, FReg::FR0, FReg::FR3);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: fmovi.32 FR0 12.500000",
        "0x006: fmovi.32 FR1 2.500000",
        "0x00c: fmovi.32 FR2 1.250000",
        "0x012: fmovi.32 FR3 0.250000",
        "0x018: fadd.32 FR0 FR0 FR1",
        "0x01b: fsub.32 FR0 FR0 FR2",
        "0x01e: fdiv.32 FR0 FR0 FR2",
        "0x021: fmul.32 FR0 FR0 FR3",
        "0x024: ret"
    );
    auto res = InterpretFPRes(code, F32(0), F32(0));
    EXPECT_EQ(res.f32, 2.75);
}

// ---------------------------------------------------------------------------
// Branches and jumps
// ---------------------------------------------------------------------------

TEST(EmitTest, Simple_Bcc)
{
    Emitter e;
    auto label = e.NewLabel();
    e.Bcc(CC::EQ, Width::W32, IReg::IR1, IReg::IR2, label);
    e.Add(Width::W32, IReg::IR1, IReg::IR1, IReg::IR2);
    e.Bind(label);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: bcc.32 eq IR1 IR2 3",
        "0x004: add.32 IR1 IR1 IR2",
        "0x007: ret"
    );

    auto res = Interpret(code, U32(2), U32(2));
    EXPECT_EQ(res.u32, 2);

    auto res2 = Interpret(code, U32(2), U32(1));
    EXPECT_EQ(res2.u32, 3);
}

TEST(EmitTest, Simple_Bcc_Loop)
{
    Emitter e;
    auto loop = e.NewLabel();
    e.Bind(loop);
    e.AddI(Width::W32, IReg::IR1, IReg::IR1, 1);
    e.Bcc(CC::LT, Width::W32, IReg::IR1, IReg::IR2, loop);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: add.32 IR1 IR1 1",
        "0x004: bcc.32 lt IR1 IR2 -8",
        "0x008: ret"
    );

    auto res = Interpret(code, U32(0), U32(100));
    EXPECT_EQ(res.u32, 100);
}

TEST(EmitTest, Simple_Jmp)
{
    Emitter e;
    auto l1 = e.NewLabel();
    auto l2 = e.NewLabel();
    auto l3 = e.NewLabel();
    e.Jmp(l2);
    e.MovImm(Width::W32, IReg::IR1, 1);
    e.Bind(l1);
    e.AddI(Width::W32, IReg::IR1, IReg::IR1, 10);
    e.Jmp(l3);
    e.MovImm(Width::W32, IReg::IR1, 2);
    e.Bind(l2);
    e.AddI(Width::W32, IReg::IR1, IReg::IR1, 20);
    e.Jmp(l1);
    e.MovImm(Width::W32, IReg::IR1, 3);
    e.Bind(l3);
    e.AddI(Width::W32, IReg::IR1, IReg::IR1, 30);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: jmp 13",
        "0x005: movi IR1 1",
        "0x007: add.32 IR1 IR1 10",
        "0x00b: jmp 13",
        "0x010: movi IR1 2",
        "0x012: add.32 IR1 IR1 20",
        "0x016: jmp -20",
        "0x01b: movi IR1 3",
        "0x01d: add.32 IR1 IR1 30",
        "0x021: ret"
    );

    auto res = Interpret(code, U32(100), U32(0));
    EXPECT_EQ(res.u32, 160);
}

// ---------------------------------------------------------------------------
// Unary operations and float math
// ---------------------------------------------------------------------------

TEST(EmitTest, Simple_Neg)
{
    Emitter e;
    e.Binary(Common::SUB, Width::W32, IReg::IR1, IReg::IRZ, IReg::IR2);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: sub.32 IR1 IRZ IR2",
        "0x003: ret"
    );

    auto res = Interpret(code, U32(0), U32(10));
    EXPECT_EQ((int32_t)res.u32, -10);
}

TEST(EmitTest, Simple_FNeg)
{
    Emitter e;
    e.Unary(FloatOperations::FNEG, Width::W32, FReg::FR0, FReg::FR1);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: fneg.32 FR0 FR1",
        "0x003: ret"
    );

    float arg1 = 1.2;
    auto res1  = InterpretFPRes(code, F32(0), F32(arg1));
    EXPECT_EQ(res1.f32, -arg1);

    float arg2 = +0.0;
    auto res2  = InterpretFPRes(code, F32(0), F32(arg2));
    EXPECT_EQ(res2.f32, -arg2);
}

TEST(EmitTest, Simple_FSqrt)
{
    Emitter e;
    e.Unary(FloatOperations::FSQRT, Width::W64, FReg::FR0, FReg::FR1);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: fsqrt.64 FR0 FR1",
        "0x003: ret"
    );

    double arg = 16.81;
    auto res   = InterpretFPRes(code, F64(0), F64(arg));
    EXPECT_EQ(res.f64, std::sqrt(arg));
}

TEST(EmitTest, Simple_FAbs)
{
    Emitter e;
    e.Unary(FloatOperations::FABS, Width::W64, FReg::FR0, FReg::FR1);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: fabs.64 FR0 FR1",
        "0x003: ret"
    );

    double arg1 = -1.2;
    auto res1   = InterpretFPRes(code, F64(0), F64(arg1));
    EXPECT_EQ(res1.f64, std::fabs(arg1));

    double arg2 = -0.0;
    auto res2   = InterpretFPRes(code, F64(0), F64(arg2));
    EXPECT_EQ(res2.f64, +0.0);
}

TEST(EmitTest, Simple_FMathUnarySin)
{
    Emitter e;
    e.FMathUnary(FloatMathOp::SIN, Width::W64, FReg::FR0, FReg::FR1);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: sin.64 FR0 FR1",
        "0x003: ret"
    );

    double arg = 0.5;
    auto res   = InterpretFPRes(code, F64(0), F64(arg));
    EXPECT_EQ(res.f64, std::sin(arg));
}

TEST(EmitTest, Simple_FMathUnaryCos32)
{
    Emitter e;
    e.FMathUnary(FloatMathOp::COS, Width::W32, FReg::FR0, FReg::FR1);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: cos.32 FR0 FR1",
        "0x003: ret"
    );

    float arg = 0.25;
    auto res  = InterpretFPRes(code, F32(0), F32(arg));
    EXPECT_EQ(res.f32, std::cos(arg));
}

TEST(EmitTest, Simple_FMathUnaryLog)
{
    Emitter e;
    e.FMathUnary(FloatMathOp::LOG, Width::W64, FReg::FR0, FReg::FR1);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: log.64 FR0 FR1",
        "0x003: ret"
    );

    double arg = 0.25;
    auto res  = InterpretFPRes(code, F64(0), F64(arg));
    EXPECT_EQ(res.f64, std::log(arg));
}

TEST(EmitTest, Simple_FMathUnaryCeil)
{
    Emitter e;
    e.FMathUnary(FloatMathOp::CEIL, Width::W64, FReg::FR0, FReg::FR1);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: ceil.64 FR0 FR1",
        "0x003: ret"
    );

    double arg = 0.25;
    auto res  = InterpretFPRes(code, F64(0), F64(arg));
    EXPECT_EQ(res.f64, std::ceil(arg));
}

TEST(EmitTest, Simple_FMathUnaryFloor)
{
    Emitter e;
    e.FMathUnary(FloatMathOp::FLOOR, Width::W64, FReg::FR0, FReg::FR1);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: floor.64 FR0 FR1",
        "0x003: ret"
    );

    double arg = 0.25;
    auto res  = InterpretFPRes(code, F64(0), F64(arg));
    EXPECT_EQ(res.f64, std::floor(arg));
}

// ---------------------------------------------------------------------------
// Conversions
// ---------------------------------------------------------------------------

TEST(EmitTest, Simple_Convert)
{
    Emitter e;
    e.Convert(ConvertType::U8, ConvertType::U32, IReg::IR1, IReg::IR1);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: convert U8 U32 IR1 IR1",
        "0x003: ret"
    );

    auto res = Interpret(code, U32(32896), U32(0), F32(0), F32(0));
    EXPECT_EQ(res.u64, U64(128).u64);
}

// ---------------------------------------------------------------------------
// Emitter state: labels, address symbols
// ---------------------------------------------------------------------------

TEST(EmitTest, LabelPosition)
{
    Emitter e;
    e.Nop();
    auto label = e.NewLabel();
    e.Bind(label);
    EXPECT_EQ(e.LabelPosition(label), 1);

    auto code = e.Build(heap);
    CHECK_DISASM(code, "0x000: nop");
}

TEST(EmitTest, AddressSymbolsAndStaticAccess)
{
    Emitter e;
    uint64_t value1 = 0xAAAA1111AAAA1111ull;
    uint64_t value2 = 0xBBBB2222BBBB2222ull;
    auto sym1       = e.NewAddressSym(reinterpret_cast<uintptr_t>(&value1));
    auto sym2       = e.NewAddressSym(reinterpret_cast<uintptr_t>(&value2));
    e.LoadStatic(LoadAccessKind::LD_64, IReg::IR1, sym1);
    e.StoreStatic(StoreAccessKind::ST_16, IReg::IR2, sym2);

    auto code = e.Build(heap);
    ASSERT_EQ(code.literals->size(), 2u);
    EXPECT_EQ(code.literals->at(0).u64, reinterpret_cast<uintptr_t>(&value1));
    EXPECT_EQ(code.literals->at(1).u64, reinterpret_cast<uintptr_t>(&value2));

    // LOAD_ADDR prints the load access kind correctly; the U16L operand prints
    // the literal-table address (STORE_ADDR is checked in
    // Disasm_StoreAddrPrintsStoreAccessKind).
    std::string prefix = "0x000: ld.addr.64 IR1 0x";
    EXPECT_EQ(LogCode(code).substr(0, prefix.size()), prefix);
}

TEST(EmitTest, Disasm_StoreAddrPrintsStoreAccessKind)
{
    Emitter e;
    uint64_t value = 0;
    auto sym       = e.NewAddressSym(reinterpret_cast<uintptr_t>(&value));
    e.StoreStatic(StoreAccessKind::ST_16, IReg::IR2, sym);
    e.StoreStatic(StoreAccessKind::ST_64, IReg::IR3, sym);

    auto code = e.Build(heap);
    // Contract: st.addr.$0stk — the store access kind must be printed with its
    // own name table ("16", "64"), like STORE_OBJ does ("st.$0stk"); the U16L
    // operand prints the literal-table address.
    auto addr = Hex(&value);
    CHECK_DISASM(
        code,
        "0x000: st.addr.16 IR2 " + addr,
        "0x004: st.addr.64 IR3 " + addr
    );
}

TEST(EmitTest, BranchIfRef)
{
    Emitter e;
    auto label = e.NewLabel();
    e.BranchIfRef(IReg::IR2, label);
    e.Ret();
    e.Bind(label);

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: branch.is.ref IR2 1",
        "0x003: ret"
    );
}

// ---------------------------------------------------------------------------
// Small single-byte instructions
// ---------------------------------------------------------------------------
TEST(EmitTest, SmallOps)
{
    Emitter e;
    e.Nop();
    e.GcPoint();
    e.CallClosure(false);
    e.CallClosure(true);
    e.CallClosureGeneric();
    e.SpawnFuture();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: nop",
        "0x001: gcpoint",
        "0x002: call.closure",
        "0x003: call.closure.sret",
        "0x004: call.closure.g",
        "0x005: spawn.future"
    );
}

TEST(EmitTest, Spawn)
{
    auto ti = NewTestTypeInfo(16);
    Emitter e;
    e.Spawn(ti);
    e.SpawnFuture();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: spawn " + Hex(ti),
        "0x009: spawn.future"
    );
}

TEST(EmitTest, DirectCalls)
{
    Emitter e;
    e.DirectCall2i(e.NewAddressSym(42));
    e.DirectCall2c(e.NewAddressSym(77));

    auto code = e.Build(heap);
    ASSERT_EQ(code.literals->size(), 2u);
    EXPECT_EQ(code.literals->at(0).u64, 42u);
    EXPECT_EQ(code.literals->at(1).u64, 77u);

    CHECK_DISASM(
        code,
        "0x000: call.2i 42",
        "0x003: call.2c 77"
    );
}

TEST(EmitTest, DirectCall2n)
{
    Emitter e;
    e.DirectCall2n(e.NewAddressSym(99), 8, 3);

    auto code = e.Build(heap);
    ASSERT_EQ(code.literals->size(), 1u);
    EXPECT_EQ(code.literals->at(0).u64, 99u);

    CHECK_DISASM(code, "0x000: call.2n 99 (3, 0x8)");
}

TEST(EmitTest, CallCFunc)
{
    Emitter e;
    e.CallCFunc(IReg::IR2, 6, 1);

    auto code = e.Build(heap);
    CHECK_DISASM(code, "0x000: call.cfunc IR2 (1, 0x6)");
}

TEST(EmitTest, VirtualCall)
{
    Emitter e;
    e.VirtualCall(5, 7, true);

    auto code = e.Build(heap);
    CHECK_DISASM(code, "0x000: vcall 0x5 0x7");
}

TEST(EmitTest, InterfaceCall)
{
    auto ti = NewTestTypeInfo(16);
    Emitter e;
    e.InterfaceCall(5, ti, true, Interpretation::StaticCallTypeInfoArgs { 3, 4 });
    e.InterfaceCall(6, ti, false, Interpretation::StaticCallTypeInfoArgs {});

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: icall.0x1 0x5 " + Hex(ti) + " outer=0x3 this=0x4",
        "0x010: icall.0x0 0x6 " + Hex(ti) + " outer=0xffff this=0xffff"
    );
}

TEST(EmitTest, InterfaceCallGeneric)
{
    Emitter e;
    e.InterfaceCallGeneric(5, true, Interpretation::StaticCallTypeInfoArgs { 3, 4 });
    e.InterfaceCallGeneric(6, false, Interpretation::StaticCallTypeInfoArgs {});

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: icall.g.0x1 0x5 outer=0x3 this=0x4",
        "0x008: icall.g.0x0 0x6 outer=0xffff this=0xffff"
    );
}

TEST(EmitTest, StringLit)
{
    auto* storage = heap.New<Interpretation::StringStorage>();
    Emitter e;
    e.StringLit(storage, 8);

    auto code = e.Build(heap);
    CHECK_DISASM(code, "0x000: string.init " + Hex(storage) + " 0x8");
}

TEST(EmitTest, ChecksAndExceptions)
{
    Emitter e;
    e.DivCheck(IReg::IR2);
    e.NullCheck(IReg::IR3);
    e.Throw(IReg::IR4);
    e.Catch(IReg::IR5);

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: divcheck IR2",
        "0x002: nullcheck IR3",
        "0x004: throw IR4",
        "0x006: catch IR5"
    );
}

TEST(EmitTest, LoadGenericTypeInfo)
{
    Emitter e;
    e.LoadGenericTypeInfo(0x1234);

    auto code = e.Build(heap);
    CHECK_DISASM(code, "0x000: load.generic.ti 0x1234");
}

TEST(EmitTest, InstanceOf)
{
    auto ti = NewTestTypeInfo(16);
    Emitter e;
    e.InstanceOf(IReg::IR1, IReg::IR2, ti);

    auto code = e.Build(heap);
    CHECK_DISASM(code, "0x000: iof IR1 IR2 " + Hex(ti));
}

TEST(EmitTest, BoxOps)
{
    auto ti = NewTestTypeInfo(16);
    Emitter e;
    e.NewBox(ti);
    e.UnboxT(IReg::IR2, 8);

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: newbox2 " + Hex(ti),
        "0x009: unbox.t IR2 0x8"
    );
}

TEST(EmitTest, Disasm_OffsetPrintsDestination)
{
    Emitter e;
    e.Offset(IReg::IR3, 7, IReg::IR5);

    auto code = e.Build(heap);
    CHECK_DISASM(code, "0x000: offset IR3 IR5 0x7 }");
}

TEST(EmitTest, StructFieldOps)
{
    auto ti = NewTestTypeInfo(16);
    Emitter e;
    e.WriteStructField(IReg::IR4, IReg::IR5, 6, ti);
    e.ReadStructField(IReg::IR1, IReg::IR2, 8, ti);

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: write.struct.field IR4 IR5 0x6 " + Hex(ti),
        "0x00b: read.struct.field IR1 IR2 0x8 " + Hex(ti)
    );
}

TEST(EmitTest, GenericAndNewOps)
{
    auto ti = NewTestTypeInfo(16);
    Emitter e;
    e.AssignGeneric(IReg::IR1, IReg::IR2, IReg::IR3);
    e.InstanceOfGeneric(IReg::IR1, IReg::IR2, IReg::IR3);
    e.NewObjGenericOnAcc(IReg::IR2);
    e.NewArrGeneric(IReg::IR2);
    e.NewArr(ti);
    e.NewObj(ti, true);
    e.TypeArg(IReg::IR1, IReg::IR2, 3);
    e.Mov(IReg::IR_ACC, IReg::IR1);

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: assign.g IR1 IR2 IR3",
        "0x003: iof.g IR1 IR2 IR3",
        "0x006: newobj.acc.g IR2",
        "0x008: newarr.g IR1, IR2, IR2",
        "0x00a: newarr IR1, IR2, " + Hex(ti),
        "0x013: newobj.pinned IR1, " + Hex(ti),
        "0x01c: type.arg IR1 IR2 3",
        "0x020: mov IR_ACC IR1"
    );
}

// ---------------------------------------------------------------------------
// Long (32-bit offset) load/store variants
// ---------------------------------------------------------------------------
TEST(EmitTest, LoadStoreDerived_LongOffsets)
{
    Emitter e;
    e.LoadDerived(LoadAccessKind::LD_64, IReg::IR1, IReg::IR2, IReg::IR3, 0x10000);
    e.StoreDerived(StoreAccessKind::ST_16, IReg::IR1, IReg::IR2, IReg::IR3, 0x10000);
    e.StoreDerived(StoreAccessKind::ST_F32, FReg::FR1, IReg::IR2, IReg::IR3, 0x10000);
    e.StoreDerived(StoreAccessKind::ST_F64, FReg::FR1, IReg::IR2, IReg::IR3, 0x10000);

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: ld.derived.64 IR1 [(IR2 IR3) 0x10000]",
        "0x007: st.derived.16 IR1 [(IR2 IR3) 0x10000]",
        "0x00e: st.derived.f32 FR1 [(IR2 IR3) 0x10000]",
        "0x015: st.derived.f64 FR1 [(IR2 IR3) 0x10000]"
    );
}

TEST(EmitTest, Disasm_LoadLongDerivedFPrintsFloatRegister)
{
    Emitter e;
    e.LoadDerived(LoadAccessKind::LD_F32, FReg::FR1, IReg::IR2, IReg::IR3, 0x10000);

    auto code = e.Build(heap);
    CHECK_DISASM(code, "0x000: ld.derived.f32 FR1 [(IR2 IR3) 0x10000]");
}

TEST(EmitTest, LoadStoreObj_LongOffsets)
{
    Emitter e;
    e.LoadObj(LoadAccessKind::LD_64, IReg::IR1, IReg::IR2, 0x10000);
    e.StoreObj(StoreAccessKind::ST_64, IReg::IR1, IReg::IR2, 0x10000);
    e.LoadObj(LoadAccessKind::LD_F64, FReg::FR1, IReg::IR2, 0x10000);
    e.StoreObj(StoreAccessKind::ST_F64, FReg::FR1, IReg::IR2, 0x10000);

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: ld.derived.64 IR1 [(IR2 IR2) 0x10000]",
        "0x007: st.derived.64 IR1 [(IR2 IR2) 0x10000]",
        "0x00e: ld.derived.f64 FR1 [(IR2 IR2) 0x10000]",
        "0x015: st.derived.f64 FR1 [(IR2 IR2) 0x10000]"
    );
}

TEST(EmitTest, LoadStoreRec_LongOffsets)
{
    Emitter e;
    e.LoadRec(LoadAccessKind::LD_64, IReg::IR1, IReg::IR2, 0x10000);
    e.StoreRec(StoreAccessKind::ST_16, IReg::IR1, IReg::IR2, 0x10000);
    e.LoadRec(LoadAccessKind::LD_F32, FReg::FR1, IReg::IR2, 0x10000);
    e.StoreRec(StoreAccessKind::ST_F64, FReg::FR1, IReg::IR2, 0x10000);

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: ld.rec.64 IR1 [IR2 0x10000]",
        "0x007: st.rec.16 IR1 [IR2 0x10000]",
        "0x00e: ld.rec.f32 FR1 [IR2 0x10000]",
        "0x015: st.rec.f64 FR1 [IR2 0x10000]"
    );
}

TEST(EmitTest, LoadStoreFrame_LongOffsets)
{
    Emitter e;
    e.LoadFrame(LoadAccessKind::LD_64, IReg::IR1, 0x10000);
    e.StoreFrame(StoreAccessKind::ST_16, IReg::IR1, 0x10000);
    e.LoadFrame(LoadAccessKind::LD_F64, FReg::FR1, 0x10000);
    e.StoreFrame(StoreAccessKind::ST_F32, FReg::FR1, 0x10000);
    e.LoadFrame(LoadAccessKind::LD_64, IReg::IR1, -8); // short encoding with negative offset

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: ld.frame.64 IR1 [65536]",
        "0x007: st.frame.16 IR1 [65536]",
        "0x00e: ld.frame.f64 FR1 [0x10000]",
        "0x015: st.frame.f32 FR1 [0x10000]",
        "0x01c: ld.frame.64 IR1 [IRZ -8]"
    );
}

// ---------------------------------------------------------------------------
// Array access
// ---------------------------------------------------------------------------
TEST(EmitTest, ArrayAccess)
{
    Emitter e;
    e.LoadArray(LoadAccessKind::LD_32, IReg::IR1, IReg::IR2, IReg::IR3);
    e.StoreArray(StoreAccessKind::ST_REF, IReg::IR1, IReg::IR2, IReg::IR3);
    e.LoadArray(LoadAccessKind::LD_F64, FReg::FR1, IReg::IR2, IReg::IR3);
    e.StoreArray(StoreAccessKind::ST_F32, FReg::FR1, IReg::IR2, IReg::IR3);

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: ld.arr.32 IR1 IR2 IR3",
        "0x003: st.arr.ref IR1 IR2 IR3]",
        "0x006: ld.arr.f64 FR1 IR2 IR3]",
        "0x009: st.arr.f32 FR1 IR2 IR3]"
    );
}

// ---------------------------------------------------------------------------
// Generic addressing
// ---------------------------------------------------------------------------
TEST(EmitTest, GenericAddressing)
{
    Emitter e;
    e.LoadGeneric(IReg::IR1, IReg::IR2, IReg::IR3, IReg::IR4);
    e.StoreGeneric(IReg::IR1, IReg::IR2, IReg::IR3, IReg::IR4, 0);
    e.LeaGeneric(IReg::IR1, IReg::IR3, IReg::IR4, 5);

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: ld.g IR1 IR4 [(IR2 IR3)]",
        "0x003: st.g0 IR1 IR2 IR3 IR4",
        "0x006: lea.g IR1 IR4 [IR3 ord=0x5]"
    );
}

TEST(EmitTest, StoreGeneric_OffsetWidths)
{
    {
        Emitter e; // 1..254 -> STORE_GENERIC_1 with an 8-bit offset
        e.StoreGeneric(IReg::IR1, IReg::IR2, IReg::IR3, IReg::IR4, 254);

        auto code = e.Build(heap);
        CHECK_DISASM(code, "0x000: st.g1 IR1 IR2 IR3 IR4 0xfe");
    }
    {
        Emitter e; // >= 255 -> STORE_GENERIC_4 with a 32-bit offset
        e.StoreGeneric(IReg::IR1, IReg::IR2, IReg::IR3, IReg::IR4, 255);

        auto code = e.Build(heap);
        CHECK_DISASM(code, "0x000: st.g4 IR1 IR2 IR3 IR4 0xff");
    }
}

TEST(EmitTest, InitGenericOption)
{
    {
        Emitter e;
        e.InitNoneGenericOpt0(IReg::IR1);

        auto code = e.Build(heap);
        CHECK_DISASM(code, "0x000: init.none.g0 IR1");
    }
    {
        Emitter e;
        e.InitSomeGeneric(IReg::IR2, IReg::IR3, true);  // OPT0
        e.InitSomeGeneric(IReg::IR4, IReg::IR5, false); // OPT1

        auto code = e.Build(heap);
        CHECK_DISASM(
            code,
            "0x000: init.some.g0 IR2 IR3",
            "0x002: init.some.g1 IR4 IR5"
        );
    }
}

TEST(EmitTest, IndexAddressing)
{
    auto ti = NewTestTypeInfo(16);
    Emitter e;
    e.LeaIndex(IReg::IR1, IReg::IR2, IReg::IR3, ti, true);
    e.LeaIndex(IReg::IR1, IReg::IR2, IReg::IR3, ti, false);
    e.LeaIndexGeneric(IReg::IR1, IReg::IR2, IReg::IR3, IReg::IR4);
    e.LeaConstIndexGeneric(IReg::IR1, IReg::IR2, -7, IReg::IR4);

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: index IR1 [IR2 IR3] " + Hex(ti),
        "0x00b: index IR1 [IR2 IR3] " + Hex(ti),
        "0x016: index.g IR1 [IR2 IR3] IR4",
        "0x019: index.g IR1 [IR2 -7] IR4"
    );
}

TEST(EmitTest, CopyDerivedOps)
{
    auto ti = NewTestTypeInfo(16);
    Emitter e;
    e.CopyDerived(IReg::IR1, IReg::IR2, IReg::IR3, IReg::IR4, ti);
    e.CopyDerivedGeneric(IReg::IR1, IReg::IR2, IReg::IR3, IReg::IR4, IReg::IR5);

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: copy.derived IR1 IR2 IR3 IR4 " + Hex(ti),
        "0x00b: copy.derived.g IR1 IR2 IR3 IR4 IR5"
    );
}

// ---------------------------------------------------------------------------
// Atomic operations
// ---------------------------------------------------------------------------
TEST(EmitTest, AtomicLoadStore)
{
    Emitter e;
    e.AtomicLoad(IReg::IR1, LoadAccessKind::LD_64, IReg::IR2, 8);
    e.AtomicStore(IReg::IR1, StoreAccessKind::ST_32, IReg::IR2, 8);

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: atomic.ld.64 IR1 [IR2 0x8]",
        "0x004: atomic.st.32 IR1 [IR2 0x8]"
    );
}

TEST(EmitTest, Disasm_AtomicOpsPrintAllOperands)
{
    Emitter e;
    e.AtomicOp(RT::Opcode::ATOMIC_FETCH_ADD_64, IReg::IR1, IReg::IR2, IReg::IR3, 8);
    e.CAS(RT::Opcode::CAS_64, IReg::IR1, IReg::IR2, IReg::IR3, IReg::IR4, 8);
    e.AtomicOp(RT::Opcode::ATOMIC_SWAP_64, IReg::IR1, IReg::IR2, IReg::IR3, 8);

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: atomic.fetch.add.64 IR1 IR3 [IR2 0x8]",
        "0x005: cas.64 IR1 IR3 IR4 [IR2 0x8]",
        "0x00a: atomic.swap.64 IR1 IR3 [IR2 0x8]"
    );
}

// ---------------------------------------------------------------------------
// Stack parameters
// ---------------------------------------------------------------------------
TEST(EmitTest, StackParams)
{
    Emitter e;
    e.StackParam(IReg::IR3, 16);
    e.StackParamF(FReg::FR3, 20);
    e.StackParamS16(-5, 24);
    e.StackParamS32(-100000, 28);

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: st.stack.param IR3 0x10",
        "0x004: st.stack.param.f FR3 0x14",
        "0x008: st.stack.param.s.16 -5 0x18",
        "0x00d: st.stack.param.s.32 -100000 0x1c"
    );
}

TEST(EmitTest, SCCImm_Literal)
{
    Emitter e;
    e.SCCImm(CC::EQ, Width::W32, IReg::IR1, IReg::IR2, 1048576);
    e.SCCImm(CC::NE, Width::W64, IReg::IR1, IReg::IR2, 1048576);

    auto code = e.Build(heap);
    ASSERT_EQ(code.literals->size(), 2u);
    EXPECT_EQ(code.literals->at(0).u64, 1048576u);
    EXPECT_EQ(code.literals->at(1).u64, 1048576u);

    CHECK_DISASM(
        code,
        "0x000: scci.32 eq IR1 IR2 1048576",
        "0x004: scci.64 ne IR1 IR2 1048576"
    );
}

// ---------------------------------------------------------------------------
// Immediate frame stores of every size
// ---------------------------------------------------------------------------
TEST(EmitTest, StoreFrameImm_EverySize)
{
    Emitter e;
    e.StoreFrameImm(StoreAccessKind::ST_8, 1, 0); // st.frame.imm.1
    e.StoreFrameImm(StoreAccessKind::ST_16, 0x1234, 4); // st.frame.imm.2
    e.StoreFrameImm(StoreAccessKind::ST_32, 0x12345678, 8); // st.frame.imm.4
    e.StoreFrameImm(StoreAccessKind::ST_64, 0x1122334455667788ull, 12); // st.frame.imm.8
    e.StoreFrameImm(StoreAccessKind::ST_REF, 0, 16); // normalized to ST_64
    e.StoreFrameImm(StoreAccessKind::ST_F32, 0x1234, 20); // normalized to ST_32
    e.StoreFrameImm(StoreAccessKind::ST_F64, 0x12345678, 24); // normalized to ST_64

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: st.frame.imm.1 8 0x1 0x0",
        "0x007: st.frame.imm.2 16 0x1234 0x4",
        "0x00f: st.frame.imm.4 32 0x12345678 0x8",
        "0x019: st.frame.imm.8 64 0x1122334455667788 0xc",
        "0x027: st.frame.imm.1 64 0x0 0x10",
        "0x02e: st.frame.imm.2 32 0x1234 0x14",
        "0x036: st.frame.imm.4 64 0x12345678 0x18"
    );
}

} // namespace Emitter
} // namespace Cbc
