#include "interpretation_loop.h"
#include "cbc/formater_rt.h"
#include "cbc/isa.h"
#include "cbc/isa_rt.h"
#include "engine/terms.h"
#include "interpreter.h"
#include "interpreter/code.h"
#include "interpreter/ectype.h"
#include "interpreter/loggers.h"
#include "runtimesupport/adapters.h"
#include "runtimesupport/runtime.h"
#include "utils/assertion.h"
#include "utils/logger.h"
#include "utils/math.h"
#include "utils/ostream.h"
#include <cmath>
#include <cstdint>

using namespace Interpretation;
using namespace Cbc::RT;
using namespace RTSupport;

extern "C" {

/// The interpretation loop can be used in two scenarios:
/// - (Main scenario) As an interpreter for the real runtime.
/// - As part of a unit test framework.
///
/// To share code, the actual `RuntimeInterface` used by the interpreter is injected
/// as a template parameter.
///
/// For the main scenario, the stack layout MUST avoid non-leaf C++ frames
/// to handle fiber stack expansion properly.
/// To achieve this, any compiled code invocation (or similar call) is not executed directly
/// from the interpreter loop (this function) itself, but rather from an assembly-written
/// function, `perform_2i_call`, that strictly tracks the frame layout.
///
/// Therefore, to perform an invocation from `perform_2i_call`, we return a `Thunk` by value,
/// which contains both the function to call and a single argument.
///
/// We restrict the number of arguments in the `Thunk` structure to 1 to allow passing the
/// structure by value via registers in the System V x64 and AArch64 ABIs.
///
/// Note that the actual calling convention of `thunk.function`
/// differs from the ASM in the unit test framework.
Interpretation::Thunk engine_interpretation_loop(
    Ectype* ectype, Frame frame, RTSupport::ThreadHandle handle, LiteralTable* literals, Decoder::ByteReader& reader0
)
{
#define NEXT goto* MAIN_TABLE[reader.PeekOpcode()]
#define NEXT_COND(successful) goto* MAIN_TABLE[(successful) ? reader.PeekOpcode() : 0]
#define MEM_NEXT goto* MEMSPACE_TABLE[reader.PeekOpcode()]
    Interpretation::Interpreter interpreter(ectype, frame, handle, literals);
    Decoder::ByteReader reader = reader0;
#ifdef NDEBUG
    #define JUMP                                                                                                       \
        reader.Advance(delta);                                                                                         \
        NEXT;
#else
    #define JUMP                                                                                                       \
        reader.Advance(delta);                                                                                         \
        pos = reader.Cursor();                                                                                         \
        NEXT;
#endif

#define CBC_RT_LABEL(opc, encoding, fmt) &&opc,
#define CBC_RT_MEM_LABEL(opc, encoding, fmt, tail) &&opc,
    static void* MAIN_TABLE[] = { CBC_RT_OPCODES(CBC_RT_LABEL) };

    static void* MEMSPACE_TABLE[] = { CBC_RT_MEMOPCODES(CBC_RT_MEM_LABEL) };

#ifdef NDEBUG
    #define LOG_INSTR
#else
    // Logging format is:
    // [int] (stack depth) < (bc pos): instruction
    #define LOG_INSTR                                                                                                  \
        do {                                                                                                           \
            logger.PrintFmt("#0x%lx < 0x%03lx: ", frame.start, pos - start);                                           \
            pos = reader.Cursor();                                                                                     \
            Cbc::RT::Log(literals, logger, args);                                                                      \
        } while (0)
    // TODO: add ectype ptr as ID of thread.
    auto start   = reader.Start();
    auto pos     = reader.Cursor();
    auto& logger = Log::interpretation.Stream(Logging::Level::TRACE);
#endif

    uint64_t memspaceOffsetAcc = 0;

    // jump to the instruction handler.
    NEXT;

// -- Main opcode table --
HALT: {
    FATAL("halt");
    return {};
}
RET: {
    auto args = B1::Decode(reader);
    LOG_INSTR;
    return {};
}
NOP: {
    auto args = B1::Decode(reader);
    LOG_INSTR;
    NEXT;
}
MOV: {
    auto args = B2rr::Decode(reader);
    LOG_INSTR;
    interpreter.Mov(args.rr.x.IR(), args.rr.y.IR());
    NEXT;
}
MOVI: {
    auto args = B2xr::Decode(reader);
    LOG_INSTR;
    interpreter.MovI(args.xr.r.IR(), MathUtils::SignExtend(static_cast<uint64_t>(args.xr.imm), 4));
    NEXT;
}
FMOV: {
    auto args = B2rr::Decode(reader);
    LOG_INSTR;
    interpreter.Mov(args.rr.x.FR(), args.rr.y.FR());
    NEXT;
}
MOVI2F: {
    auto args = B2rr::Decode(reader);
    LOG_INSTR;
    interpreter.Mov(args.rr.x.FR(), args.rr.y.IR());
    NEXT;
}
MOVF2I: {
    auto args = B2rr::Decode(reader);
    LOG_INSTR;
    interpreter.Mov(args.rr.x.IR(), args.rr.y.FR());
    NEXT;
}
GC_POINT: {
    auto args = B1::Decode(reader);
    LOG_INSTR;
    bool is_sp = RTSupport::Execution::IsPendingSafePoint();
    if (!is_sp) {
        NEXT;
    }

    reader0 = reader;

    return { RTSupport::Execution::GcPointTrampoline(), RTSupport::Execution::GcPoint() };
}
FMOVI32: {
    auto args = B6xri32::Decode(reader);
    LOG_INSTR;
    interpreter.MovI(args.xr.r.FR(), args.imm32.fimm);
    NEXT;
}
FMOVI64: {
    auto args = B10xri64::Decode(reader);
    LOG_INSTR;
    interpreter.MovI(args.xr.r.FR(), args.imm64.dimm);
    NEXT;
}
BCC32I: {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    int64_t delta = interpreter.template Bcc<ImmKind::VALUE, Width::W32>(
        args.xi12.imm4.CC(), args.rr.x, args.rr.y, args.xi12.imm12
    );
    JUMP;
}
BCC32L: {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    int64_t delta = interpreter.template Bcc<ImmKind::LITERAL, Width::W32>(
        args.xi12.imm4.CC(), args.rr.x, args.rr.y, args.xi12.imm12
    );
    JUMP;
}
BCC64I: {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    int64_t delta = interpreter.template Bcc<ImmKind::VALUE, Width::W64>(
        args.xi12.imm4.CC(), args.rr.x, args.rr.y, args.xi12.imm12
    );
    JUMP;
}
BCC64L: {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    int64_t delta = interpreter.template Bcc<ImmKind::LITERAL, Width::W64>(
        args.xi12.imm4.CC(), args.rr.x, args.rr.y, args.xi12.imm12
    );
    JUMP;
}
BCCI32I: {
    auto args = B5xi12ri12::Decode(reader);
    LOG_INSTR;
    int64_t delta = interpreter.template BccImm<ImmKind::VALUE, ImmKind::VALUE, Width::W32>(
        args.xi12.imm4.CC(), args.ri12.r.IR(), args.ri12.imm12, args.xi12.imm12
    );
    JUMP;
}
BCCI64I: {
    auto args = B5xi12ri12::Decode(reader);
    LOG_INSTR;
    int64_t delta = interpreter.template BccImm<ImmKind::VALUE, ImmKind::VALUE, Width::W64>(
        args.xi12.imm4.CC(), args.ri12.r.IR(), args.ri12.imm12, args.xi12.imm12
    );
    JUMP;
}
BCCI32L: {
    auto args = B5xi12ri12::Decode(reader);
    LOG_INSTR;
    int64_t delta = interpreter.template BccImm<ImmKind::VALUE, ImmKind::LITERAL, Width::W32>(
        args.xi12.imm4.CC(), args.ri12.r.IR(), args.ri12.imm12, args.xi12.imm12
    );
    JUMP;
}
BCCI64L: {
    auto args = B5xi12ri12::Decode(reader);
    LOG_INSTR;
    int64_t delta = interpreter.template BccImm<ImmKind::VALUE, ImmKind::LITERAL, Width::W64>(
        args.xi12.imm4.CC(), args.ri12.r.IR(), args.ri12.imm12, args.xi12.imm12
    );
    JUMP;
}
BCCL32I: {
    auto args = B5xi12ri12::Decode(reader);
    LOG_INSTR;
    int64_t delta = interpreter.template BccImm<ImmKind::LITERAL, ImmKind::VALUE, Width::W32>(
        args.xi12.imm4.CC(), args.ri12.r.IR(), args.ri12.imm12, args.xi12.imm12
    );
    JUMP;
}
BCCL64I: {
    auto args = B5xi12ri12::Decode(reader);
    LOG_INSTR;
    int64_t delta = interpreter.template BccImm<ImmKind::LITERAL, ImmKind::VALUE, Width::W64>(
        args.xi12.imm4.CC(), args.ri12.r.IR(), args.ri12.imm12, args.xi12.imm12
    );
    JUMP;
}
BCCL32L: {
    auto args = B5xi12ri12::Decode(reader);
    LOG_INSTR;
    int64_t delta = interpreter.template BccImm<ImmKind::LITERAL, ImmKind::LITERAL, Width::W32>(
        args.xi12.imm4.CC(), args.ri12.r.IR(), args.ri12.imm12, args.xi12.imm12
    );
    JUMP;
}
BCCL64L: {
    auto args = B5xi12ri12::Decode(reader);
    LOG_INSTR;
    int64_t delta = interpreter.template BccImm<ImmKind::LITERAL, ImmKind::LITERAL, Width::W64>(
        args.xi12.imm4.CC(), args.ri12.r.IR(), args.ri12.imm12, args.xi12.imm12
    );
    JUMP;
}
JMP32: {
    auto args = B5i32::Decode(reader);
    LOG_INSTR;
    int64_t delta = interpreter.Jmp(args.imm32.imm);
    JUMP;
}
BIN32: {
    auto args = B3xrrr::Decode(reader);
    LOG_INSTR;
    bool successful =
        interpreter.template Binary<Width::W32>(args.xr.imm.Common(), args.xr.r.IR(), args.rr.x.IR(), args.rr.y.IR());
    NEXT_COND(successful);
}
BIN64: {
    auto args = B3xrrr::Decode(reader);
    LOG_INSTR;
    bool successful =
        interpreter.template Binary<Width::W64>(args.xr.imm.Common(), args.xr.r.IR(), args.rr.x.IR(), args.rr.y.IR());
    NEXT_COND(successful);
}
BINI32I: {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.template BinaryImm<ImmKind::VALUE, Width::W32>(
        args.xi12.imm4.Common(), args.rr.x.IR(), args.rr.y.IR(), args.xi12.imm12
    );
    NEXT_COND(successful);
}
BINI64I: {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.template BinaryImm<ImmKind::VALUE, Width::W64>(
        args.xi12.imm4.Common(), args.rr.x.IR(), args.rr.y.IR(), args.xi12.imm12
    );
    NEXT_COND(successful);
}
BINI32L: {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.template BinaryImm<ImmKind::LITERAL, Width::W32>(
        args.xi12.imm4.Common(), args.rr.x.IR(), args.rr.y.IR(), args.xi12.imm12
    );
    NEXT_COND(successful);
}
BINI64L: {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.template BinaryImm<ImmKind::LITERAL, Width::W64>(
        args.xi12.imm4.Common(), args.rr.x.IR(), args.rr.y.IR(), args.xi12.imm12
    );
    NEXT_COND(successful);
}
FBIN32: {
    auto args = B3xrrr::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.template Binary<Width::W32>(
        args.xr.imm.FloatOperations(), args.xr.r.FR(), args.rr.x.FR(), args.rr.y.FR()
    );
    NEXT_COND(successful);
}
FBIN64: {
    auto args = B3xrrr::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.template Binary<Width::W64>(
        args.xr.imm.FloatOperations(), args.xr.r.FR(), args.rr.x.FR(), args.rr.y.FR()
    );
    NEXT_COND(successful);
}
FUN32: {
    auto args = B3xrrr::Decode(reader);
    LOG_INSTR;
    bool successful =
        interpreter.template Unary<Width::W32>(args.xr.imm.FloatOperations(), args.xr.r.FR(), args.rr.y.FR());
    NEXT_COND(successful);
}
FUN64: {
    auto args = B3xrrr::Decode(reader);
    LOG_INSTR;
    bool successful =
        interpreter.template Unary<Width::W64>(args.xr.imm.FloatOperations(), args.xr.r.FR(), args.rr.y.FR());
    NEXT_COND(successful);
}
NEWOBJ: {
    auto args = B9i64::Decode(reader);
    LOG_INSTR;
    auto type = TypeInfo(static_cast<uintptr_t>(args.imm64.imm));

    // Puts result to `IR1`.
    auto func = RTSupport::Execution::AllocateObjectInstance();

    reader0 = reader; // save current pc

    return { func, type.Raw() };
}
NEWBOX: {
    auto args = B2xr::Decode(reader);
    LOG_INSTR;
    auto btype = builtinTypeInfos[args.xr.imm];

    // Puts result to `IR1`.
    auto func = RTSupport::Execution::AllocateObjectInstanceAcc();

    reader0 = reader; // save current pc

    return { func, btype.Raw() };
}
NEWBOX2: {
    auto args = B9i64::Decode(reader);
    LOG_INSTR;
    auto type = TypeInfo(static_cast<uintptr_t>(args.imm64.imm));

    // Puts result to `IR1`.
    auto func = RTSupport::Execution::AllocateObjectInstanceAcc();

    reader0 = reader; // save current pc

    return { func, type.Raw() };
}
READ_STRUCT_FIELD: {
    auto args = StructFieldOp::Decode(reader);
    LOG_INSTR;
    auto dst   = ectype->GetPrimitive(args.rr.x.IR()).u64;
    auto base  = ectype->GetReference(args.rr.y.IR());
    auto field = ectype->GetPrimitive(args.field.x.IR()).u64;
    RTSupport::Execution::ReadStructField(dst, base, field, args.ti, handle);
    NEXT;
}
WRITE_STRUCT_FIELD: {
    auto args = StructFieldOp::Decode(reader);
    LOG_INSTR;
    auto src   = ectype->GetPrimitive(args.rr.x.IR()).u64;
    auto base  = ectype->GetReference(args.rr.y.IR());
    auto field = ectype->GetPrimitive(args.field.x.IR()).u64;
    RTSupport::Execution::WriteStructField(src, base, field, args.ti, handle);
    NEXT;
}
INITCLOSURE: {
    auto args = B1::Decode(reader);
    LOG_INSTR;

    struct ClosureObj {
        void* header;
        void* generic;
        void* instantiated;
    };

    /// CBC-provided closures are always have two fields reserved with
    /// function pointers to generic and instantiated versions of function.
    /// Two fields are always reserved for this functions, even if `instantiated` part is never used.
    auto closure = reinterpret_cast<ClosureObj*>(ectype->GetReference(IReg::IR1).value);

    closure->generic      = Adapters::GetDynCallTrampoline(0);
    closure->instantiated = Adapters::GetDynCallTrampoline(1);
    NEXT;
}
SPAWN: {
    auto args = B9i64::Decode(reader);
    LOG_INSTR;
    auto type = TypeInfo(static_cast<uintptr_t>(args.imm64.imm));

    // TODO: it seems that spawn could be called directly
    // Puts result to `IR1`.
    auto func = RTSupport::Execution::Spawn();

    reader0 = reader; // save current pc

    return { func, type.Raw() };
}
NEWARR: {
    auto args = B9i64::Decode(reader);
    LOG_INSTR;
    auto type = TypeInfo(static_cast<uintptr_t>(args.imm64.imm));

    // Puts result to `IR1`, expects length to be passed on `IR2`.
    auto func = RTSupport::Execution::AllocateArrayInstance();

    reader0 = reader; // save current pc

    return { func, type.Raw() };
}
LOAD_ADDR: {
    auto args = B2xr::Decode(reader);
    LOG_INSTR;
    auto location   = literals->at(reader.Read16()).u64;
    bool successful = interpreter.LoadAddr(args.xr.imm.LDK(), args.xr.r, location);
    NEXT_COND(successful);
}
STORE_ADDR: {
    auto args = B2xr::Decode(reader);
    LOG_INSTR;
    auto location   = literals->at(reader.Read16()).u64;
    bool successful = interpreter.StoreAddr(args.xr.imm.STK(), args.xr.r, location);
    NEXT_COND(successful);
}
LOAD_OBJ_F:
LOAD_OBJ: {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.LoadObj(args.xi12.imm4.LDK(), args.rr.x, args.rr.y.IR(), args.xi12.imm12);
    NEXT_COND(successful);
}
STORE_OBJ_F:
STORE_OBJ: {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.StoreObj(args.xi12.imm4.STK(), args.rr.x, args.rr.y.IR(), args.xi12.imm12);
    NEXT_COND(successful);
}
LOAD_ARR_F:
LOAD_ARR: {
    auto args = B3xrrr::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.LoadArray(args.xr.imm.LDK(), args.xr.r, args.rr.x.IR(), args.rr.y.IR());
    NEXT_COND(successful);
}
STORE_ARR_F:
STORE_ARR: {
    auto args = B3xrrr::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.StoreArray(args.xr.imm.STK(), args.xr.r, args.rr.x.IR(), args.rr.y.IR());
    NEXT_COND(successful);
}
LOAD_REC_F:
LOAD_REC: {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.LoadRec(args.xi12.imm4.LDK(), args.rr.x, args.rr.y.IR(), args.xi12.imm12);
    NEXT_COND(successful);
}
STORE_REC_F:
STORE_REC: {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.StoreRec(args.xi12.imm4.STK(), args.rr.x, args.rr.y.IR(), args.xi12.imm12);
    NEXT_COND(successful);
}
LOAD_FRAME_F:
LOAD_FRAME: {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.LoadFrame(args.xi12.imm4.LDK(), args.rr.x, args.xi12.imm12);
    NEXT_COND(successful);
}
STORE_FRAME_F:
STORE_FRAME: {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.StoreFrame(args.xi12.imm4.STK(), args.rr.x, args.xi12.imm12);
    NEXT_COND(successful);
}
PREP_TYPED: {
    auto args = B13i64i32::Decode(reader);
    LOG_INSTR;
    auto typedOffset = args.imm32.imm;
    auto typeInfo    = TypeInfo(static_cast<uintptr_t>(args.imm64.imm));
    MetaInfo::VisitReferences(typeInfo, [&](uint32_t offset) {
        interpreter.StoreFrameImm(StoreAccessKind::ST_64, 0, typedOffset + offset);
    });
    NEXT;
}
SCC32: {
    auto args = B3xrrr::Decode(reader);
    LOG_INSTR;
    interpreter.template SCC<Width::W32>(args.xr.imm.CC(), args.xr.r.IR(), args.rr.x.IR(), args.rr.y.IR());
    NEXT;
}
SCC64: {
    auto args = B3xrrr::Decode(reader);
    LOG_INSTR;
    interpreter.template SCC<Width::W64>(args.xr.imm.CC(), args.xr.r.IR(), args.rr.x.IR(), args.rr.y.IR());
    NEXT;
}
FSCC32: {
    auto args = B3xrrr::Decode(reader);
    LOG_INSTR;
    interpreter.template SCC<Width::W32>(args.xr.imm.CC(), args.xr.r.IR(), args.rr.x.FR(), args.rr.y.FR());
    NEXT;
}
FSCC64: {
    auto args = B3xrrr::Decode(reader);
    LOG_INSTR;
    interpreter.template SCC<Width::W64>(args.xr.imm.CC(), args.xr.r.IR(), args.rr.x.FR(), args.rr.y.FR());
    NEXT;
}
SCCI32I: {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    interpreter.template SCCImm<ImmKind::VALUE, Width::W32>(
        args.xi12.imm4.CC(), args.rr.x.IR(), args.rr.y.IR(), args.xi12.imm12
    );
    NEXT;
}
SCCI64I: {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    interpreter.template SCCImm<ImmKind::VALUE, Width::W64>(
        args.xi12.imm4.CC(), args.rr.x.IR(), args.rr.y.IR(), args.xi12.imm12
    );
    NEXT;
}
SCCI32L: {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    interpreter.template SCCImm<ImmKind::LITERAL, Width::W32>(
        args.xi12.imm4.CC(), args.rr.x.IR(), args.rr.y.IR(), args.xi12.imm12
    );
    NEXT;
}
SCCI64L: {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    interpreter.template SCCImm<ImmKind::LITERAL, Width::W64>(
        args.xi12.imm4.CC(), args.rr.x.IR(), args.rr.y.IR(), args.xi12.imm12
    );
    NEXT;
}
OFFSET: {
    auto args = Offset::Decode(reader);
    LOG_INSTR;
    auto dst  = args.rr.x.IR();
    auto ti   = TypeInfo(ectype->GetPrimitive(args.rr.y.IR()).u64);
    auto offs = RTSupport::Execution::GetFieldOffset(ti, args.idx, false);
    ectype->Put(dst, Value::Primitive { offs });
    NEXT;
}
TYPE_ARG: {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    auto dst = args.rr.x.IR();
    auto ti  = args.rr.y.IR();
    auto idx = args.xi12.imm12;

    auto typeInfo = TypeInfo(ectype->GetPrimitive(ti).u64);
    auto res      = Execution::TypeArg(typeInfo, idx);
    ectype->Put(dst, Value::Primitive { res.UInt() });

    NEXT;
}
CONVERT: {
    auto args = B3xxrr::Decode(reader);
    LOG_INSTR;
    interpreter.Convert(args.xx.imm1.ConvertType(), args.xx.imm2.ConvertType(), args.rr.x, args.rr.y);
    NEXT;
}
BFXS: {
    auto args = BFX::Decode(reader);
    LOG_INSTR;
    interpreter.BitFieldExtract(args.rr.x, args.rr.y, args.offs, args.size, true);
    NEXT;
}
BFXZ: {
    auto args = BFX::Decode(reader);
    LOG_INSTR;
    interpreter.BitFieldExtract(args.rr.x, args.rr.y, args.offs, args.size, false);
    NEXT;
}
DIRECT_CALL_2I: {
    auto args = B3xi12::Decode(reader);
    LOG_INSTR;
    uint16_t imm = args.xi12.imm12;
    auto fuh     = reinterpret_cast<FunctionHandle*>(literals->at(imm).uintptr);
    // For proper support of fibers, the following call MUST drop the current frame.
    // This can not be guaranteed by C++ compiler consistently, because TCO
    // is not guaranteed and `mustcall` attribute is not supported
    // fully by gcc/clang compilers.
    //
    // Instead, the following call will drop the current frame manually
    // (outside of unit-test framework).

    reader0 = reader; // save current pc

    return { fuh->i2call, reinterpret_cast<void*>(fuh) };
}
DIRECT_CALL_2C: {
    auto args = B3xi12::Decode(reader);
    LOG_INSTR;
    uint16_t imm = args.xi12.imm12;
    auto target  = literals->at(imm).uintptr;
    // For proper support of fibers, the following call MUST drop the current frame.
    // This can not be guaranteed by C++ compiler consistently, because TCO
    // is not guaranteed and `mustcall` attribute is not supported
    // fully by gcc/clang compilers.
    //
    // Instead, the following call will drop the current frame manually
    // (outside of unit-test framework).

    reader0 = reader; // save current pc

    return { Adapters::GenericI2CCallInstance(), reinterpret_cast<void*>(target) };
}
VIRTUAL_CALL: {
    auto args = B5i16i16::Decode(reader);
    LOG_INSTR;
    auto vnum      = args.imm1.imm;
    auto extDefNum = args.imm2.imm;

    auto reference = ectype->GetReference(IReg::IR1);

    // For proper support of fibers, the following call MUST drop the current frame.
    // This can not be guaranteed by C++ compiler consistently, because TCO
    // is not guaranteed and `mustcall` attribute is not supported
    // fully by gcc/clang compilers.
    //
    // Instead, the following call will drop the current frame manually
    // (outside of unit-test framework).

    reader0 = reader; // save current pc

    return Execution::GetVirtualThunk(reference, extDefNum, vnum);
}

INTERFACE_CALL: {
    auto args = B11i16i64::Decode(reader);
    LOG_INSTR;
    auto num       = args.imm16.imm;
    auto typeInfo  = TypeInfo(static_cast<uintptr_t>(args.imm64.imm));
    auto reference = ectype->GetReference(IReg::IR1);

    // For proper support of fibers, the following call MUST drop the current frame.
    // This can not be guaranteed by C++ compiler consistently, because TCO
    // is not guaranteed and `mustcall` attribute is not supported
    // fully by gcc/clang compilers.
    //
    // Instead, the following call will drop the current frame manually
    // (outside of unit-test framework).

    reader0 = reader; // save current pc

    return Execution::GetInterfaceThunk(reference, typeInfo, num);
}

STRING_INIT: {
    auto args = B13i64i32::Decode(reader);
    LOG_INSTR;
    auto ref  = reinterpret_cast<StringStorage*>(args.imm64.imm);
    auto offs = args.imm32.imm;

    struct CJString {
        StringStorage* str;
        uint32_t start;
        uint32_t length;
    };

    /// TODO: more effective string encoding?
    auto recordLoc    = reinterpret_cast<CJString*>(frame.start + offs);
    recordLoc->str    = ref;
    recordLoc->start  = 0;
    recordLoc->length = ref->size;
    NEXT;
}

NULLCHECK: {
    auto args = B2xr::Decode(reader);
    LOG_INSTR;
    auto ref = ectype->GetReference(args.xr.r.IR());
    if (ref.value == 0) {
        FATAL("null check failed"); // TODO: throw exception
    }
    NEXT;
}

DIVCHECK: {
    auto args = B2xr::Decode(reader);
    LOG_INSTR;
    auto div = ectype->GetPrimitive(args.xr.r.IR());
    if (div.u64 == 0) {
        FATAL("div check failed"); // TODO: throw exception
    }
    NEXT;
}

LOAD_GENERIC_TI: {
    auto args = B9i64::Decode(reader);
    LOG_INSTR;
    auto termValue = args.imm64.imm;
    Engine::Term term { nullptr };
    static_assert(sizeof(term) == sizeof(termValue));
    memcpy(&term, &termValue, sizeof(termValue));
    auto ti = Execution::LoadTypeInfo(term.AsGlobal(), ectype, reinterpret_cast<void*>(frame.start));
    ectype->Put(IReg::IR1, Value::Primitive { .u64 = reinterpret_cast<uintptr_t>(ti.Raw()) });
    NEXT;
}

LOAD_TI: {
    auto args = B9i64::Decode(reader);
    LOG_INSTR;
    auto ti = args.imm64.imm;
    ectype->Put(IReg::IR1, Value::Primitive { .u64 = ti });
    NEXT;
}

IOF: {
    auto args = IOF::Decode(reader);
    LOG_INSTR;
    auto dst      = args.rr.x.IR();
    auto ref      = ectype->GetReference(args.rr.y.IR());
    auto typeInfo = TypeInfo(static_cast<uintptr_t>(args.imm64));
    ectype->Put(dst, Value::Primitive { .u64 = Execution::IsInstanceOf(ref, typeInfo) });
    NEXT;
}

THROW: {
    auto args = B2xr::Decode(reader);
    LOG_INSTR;
    auto ref = ectype->GetReference(args.xr.r.IR());
    if (ref.value == 0) {
        FATAL("unexpected null in THROW");
    }
    uintptr_t** header = reinterpret_cast<uintptr_t**>(ref.value);
    auto typeInfo      = TypeInfo(*header);
    FATAL("Throw %s", MetaInfo::GetName(typeInfo)); // TODO: throw exception
}

MEMSPACE: {
    auto args = B1::Decode(reader);
    LOG_INSTR;
    memspaceOffsetAcc = 0;
    MEM_NEXT;
}

    // -- MemSpace opcode table --

MEM_HALT: {
    FATAL("halt");
    return {};
}

OFFS16: {
    auto args = M3i16::Decode(reader);
    LOG_INSTR;
    memspaceOffsetAcc += interpreter.MemOffset(args.imm16);
    MEM_NEXT;
}
OFFS32: {
    auto args = M5i32::Decode(reader);
    LOG_INSTR;
    memspaceOffsetAcc += interpreter.MemOffset(args.imm32);
    MEM_NEXT;
}
OFFS64: {
    auto args = M9i64::Decode(reader);
    LOG_INSTR;
    memspaceOffsetAcc += interpreter.MemOffset(args.imm64);
    MEM_NEXT;
}
OFFS_REG: {
    auto args = M2xr::Decode(reader);
    LOG_INSTR;
    memspaceOffsetAcc += interpreter.MemOffsetReg(args.xr.r.IR());
    MEM_NEXT;
}
OFFS_REG_IDX64: {
    auto args = M10xri64::Decode(reader);
    LOG_INSTR;
    memspaceOffsetAcc += interpreter.MemOffsetReg(args.xr.r.IR()) * interpreter.MemOffset(args.imm64.imm);
    MEM_NEXT;
}
R_READ_STRUCT: {
    auto args = MStructFieldOp::Decode(reader);
    LOG_INSTR;
    auto dst   = ectype->GetPrimitive(args.rr.x.IR()).u64;
    auto base  = ectype->GetReference(args.rr.y.IR());
    auto field = base.value + memspaceOffsetAcc;
    RTSupport::Execution::ReadStructField(dst, base, field, args.ti, handle);
    NEXT;
}
R_WRITE_STRUCT: {
    auto args = MStructFieldOp::Decode(reader);
    LOG_INSTR;
    auto src   = ectype->GetPrimitive(args.rr.x.IR()).u64;
    auto base  = ectype->GetReference(args.rr.y.IR());
    auto field = base.value + memspaceOffsetAcc;
    RTSupport::Execution::WriteStructField(src, base, field, args.ti, handle);
    NEXT;
}

DLD_GENERIC: {
    auto args = M3rrrr::Decode(reader);
    LOG_INSTR;
    // TODO: reorder args, so it would require less bit-shifting
    auto derivedReg = args.rr1.x.IR();
    auto tiReg      = args.rr1.y.IR();
    auto dstReg     = args.rr2.x.IR();
    auto baseReg    = args.rr2.y.IR();
    auto derived    = ectype->GetReference(derivedReg);

    auto typeInfo = TypeInfo(ectype->GetPrimitive(tiReg).u64);
    if (RTSupport::Execution::IsReference(typeInfo)) {
        auto base = ectype->GetReference(baseReg);
        auto obj  = RTSupport::Execution::ReadObjectInstance(base, derived.value + memspaceOffsetAcc, handle);
        ectype->Put(dstReg, obj);
        NEXT;
    } else {
        // The operation require two steps: box allocation and ReadGeneric invocation.
        // Because box allocation can provoke GC or throw, we should not perform it with C++ frame on the stack.
        ectype->Put(IReg::IR_ACC, Value::Reference { derived.value + memspaceOffsetAcc });

        // on x64 and aarch64 pointers are 48-bit values
        uint64_t rawTi  = typeInfo.UInt();
        uint64_t packed = 0ULL | dstReg | (baseReg << 4) | (IReg::IR_ACC << 8) | rawTi << 12;
        reader0         = reader;
        return { .function = RTSupport::Execution::LoadGeneric(), .argUInt = packed };
    }
}
DST_GENERIC: {
    auto args = M3rrrr::Decode(reader);
    LOG_INSTR;
    auto derivedReg = args.rr1.x.IR();
    auto tiReg      = args.rr1.y.IR();
    auto srcReg     = args.rr2.x.IR();
    auto baseReg    = args.rr2.y.IR();

    auto base    = ectype->GetReference(baseReg);
    auto derived = ectype->GetReference(derivedReg);
    auto obj     = ectype->GetReference(srcReg);

    auto typeInfo = TypeInfo(ectype->GetPrimitive(tiReg).u64);
    if (RTSupport::Execution::IsReference(typeInfo)) {
        RTSupport::Execution::WriteObjectInstance(base, derived.value + memspaceOffsetAcc, obj, handle);
        NEXT;
    } else {
        uint32_t size = RTSupport::MetaInfo::GetTypeSize(typeInfo);
        RTSupport::Execution::WriteGeneric(base, derived.value + memspaceOffsetAcc, obj, size, handle);
        NEXT;
    }
}

GENERIC_FIELD: {
    auto args = M6rri32::Decode(reader);
    LOG_INSTR;
    auto ti            = TypeInfo(ectype->GetPrimitive(args.rr.x.IR()).u64);
    auto offs          = RTSupport::Execution::GetFieldOffset(ti, args.imm32.imm, false);
    memspaceOffsetAcc += offs;
    MEM_NEXT;
}

#define RLD(ldk)                                                                                                       \
    RLD_##ldk:                                                                                                         \
    {                                                                                                                  \
        auto args = M2rr::Decode(reader);                                                                              \
        LOG_INSTR;                                                                                                     \
        bool successful =                                                                                              \
            interpreter.LoadObj(Format::LoadAccessKind::LD_##ldk, args.rr.x, args.rr.y.IR(), memspaceOffsetAcc);       \
        NEXT_COND(successful);                                                                                         \
    }
    RLD(U8)
    RLD(U16)
    RLD(32)
    RLD(S8)
    RLD(S16)
    RLD(F32)
    RLD(F64)
    RLD(64)
    RLD(S32TO64)
    RLD(REF)
#undef RLD

#define RST(stk)                                                                                                       \
    RST_##stk:                                                                                                         \
    {                                                                                                                  \
        auto args = M2rr::Decode(reader);                                                                              \
        LOG_INSTR;                                                                                                     \
        bool successful =                                                                                              \
            interpreter.StoreObj(Format::StoreAccessKind::ST_##stk, args.rr.x, args.rr.y.IR(), memspaceOffsetAcc);     \
        NEXT_COND(successful);                                                                                         \
    }
    RST(8)
    RST(16)
    RST(32)
    RST(64)
    RST(REF)
    RST(F32)
    RST(F64)
#undef RST

#define RSTI(memSize, immSize, encoding)                                                                               \
    RSTI_##memSize##_##immSize:                                                                                        \
    {                                                                                                                  \
        auto args = encoding::Decode(reader);                                                                          \
        LOG_INSTR;                                                                                                     \
        uint64_t imm = MathUtils::SignExtend(static_cast<uint64_t>(args.imm##immSize.imm), immSize);                   \
        IReg base    = args.xr.r.IR();                                                                                 \
        bool successful =                                                                                              \
            interpreter.StoreObjImm(Format::StoreAccessKind::ST_##memSize, base, memspaceOffsetAcc, imm);              \
        NEXT_COND(successful);                                                                                         \
    }
    RSTI(8, 8, M3xri8)
    RSTI(16, 8, M3xri8)
    RSTI(16, 16, M4xri16)
    RSTI(32, 8, M3xri8)
    RSTI(32, 16, M4xri16)
    RSTI(32, 32, M6xri32)
    RSTI(64, 8, M3xri8)
    RSTI(64, 16, M4xri16)
    RSTI(64, 32, M6xri32)
    RSTI(64, 64, M10xri64)
#undef RSTI

#define DLD(ldk)                                                                                                       \
    DLD_##ldk:                                                                                                         \
    {                                                                                                                  \
        auto args = M3xrrr::Decode(reader);                                                                            \
        LOG_INSTR;                                                                                                     \
        bool successful = interpreter.LoadDerived(                                                                     \
            Format::LoadAccessKind::LD_##ldk, args.xr.r, args.rr.x.IR(), args.rr.y.IR(), memspaceOffsetAcc             \
        );                                                                                                             \
        NEXT_COND(successful);                                                                                         \
    }
    DLD(U8)
    DLD(U16)
    DLD(32)
    DLD(S8)
    DLD(S16)
    DLD(F32)
    DLD(F64)
    DLD(64)
    DLD(S32TO64)
    DLD(REF)
#undef DLD

#define DST(stk)                                                                                                       \
    DST_##stk:                                                                                                         \
    {                                                                                                                  \
        auto args = M3xrrr::Decode(reader);                                                                            \
        LOG_INSTR;                                                                                                     \
        bool successful = interpreter.StoreDerived(                                                                    \
            Format::StoreAccessKind::ST_##stk, args.xr.r, args.rr.x.IR(), args.rr.y.IR(), memspaceOffsetAcc            \
        );                                                                                                             \
        NEXT_COND(successful);                                                                                         \
    }
    DST(8)
    DST(16)
    DST(32)
    DST(64)
    DST(REF)
    DST(F32)
    DST(F64)
#undef DST

#define DSTI(memSize, immSize, encoding)                                                                               \
    DSTI_##memSize##_##immSize:                                                                                        \
    {                                                                                                                  \
        auto args = encoding::Decode(reader);                                                                          \
        LOG_INSTR;                                                                                                     \
        uint64_t imm = MathUtils::SignExtend(static_cast<uint64_t>(args.imm##immSize.imm), immSize);                   \
        IReg base    = args.rr.x.IR();                                                                                 \
        IReg derived = args.rr.y.IR();                                                                                 \
        bool successful =                                                                                              \
            interpreter.StoreDerivedImm(Format::StoreAccessKind::ST_##memSize, base, derived, memspaceOffsetAcc, imm); \
        NEXT_COND(successful);                                                                                         \
    }
    DSTI(8, 8, M3rri8)
    DSTI(16, 8, M3rri8)
    DSTI(16, 16, M4rri16)
    DSTI(32, 8, M3rri8)
    DSTI(32, 16, M4rri16)
    DSTI(32, 32, M6rri32)
    DSTI(64, 8, M3rri8)
    DSTI(64, 16, M4rri16)
    DSTI(64, 32, M6rri32)
    DSTI(64, 64, M10rri64)
#undef DSTI

#define SLD(ldk)                                                                                                       \
    SLD_##ldk:                                                                                                         \
    {                                                                                                                  \
        auto args = M2rr::Decode(reader);                                                                              \
        LOG_INSTR;                                                                                                     \
        bool successful =                                                                                              \
            interpreter.LoadRec(Format::LoadAccessKind::LD_##ldk, args.rr.x, args.rr.y.IR(), memspaceOffsetAcc);       \
        NEXT_COND(successful);                                                                                         \
    }
    SLD(U8)
    SLD(U16)
    SLD(32)
    SLD(S8)
    SLD(S16)
    SLD(F32)
    SLD(F64)
    SLD(64)
    SLD(S32TO64)
    SLD(REF)
#undef SLD

#define SST(stk)                                                                                                       \
    SST_##stk:                                                                                                         \
    {                                                                                                                  \
        auto args = M2rr::Decode(reader);                                                                              \
        LOG_INSTR;                                                                                                     \
        bool successful =                                                                                              \
            interpreter.StoreRec(Format::StoreAccessKind::ST_##stk, args.rr.x, args.rr.y.IR(), memspaceOffsetAcc);     \
        NEXT_COND(successful);                                                                                         \
    }
    SST(8)
    SST(16)
    SST(32)
    SST(64)
    SST(REF)
    SST(F32)
    SST(F64)
#undef SST

#define SSTI(memSize, immSize, encoding)                                                                               \
    SSTI_##memSize##_##immSize:                                                                                        \
    {                                                                                                                  \
        auto args = encoding::Decode(reader);                                                                          \
        LOG_INSTR;                                                                                                     \
        uint64_t imm = MathUtils::SignExtend(static_cast<uint64_t>(args.imm##immSize.imm), immSize);                   \
        IReg base    = args.xr.r.IR();                                                                                 \
        bool successful =                                                                                              \
            interpreter.StoreRecImm(Format::StoreAccessKind::ST_##memSize, base, memspaceOffsetAcc, imm);              \
        NEXT_COND(successful);                                                                                         \
    }
    SSTI(8, 8, M3xri8)
    SSTI(16, 8, M3xri8)
    SSTI(16, 16, M4xri16)
    SSTI(32, 8, M3xri8)
    SSTI(32, 16, M4xri16)
    SSTI(32, 32, M6xri32)
    SSTI(64, 8, M3xri8)
    SSTI(64, 16, M4xri16)
    SSTI(64, 32, M6xri32)
    SSTI(64, 64, M10xri64)
#undef SSTI

#define FLD(ldk)                                                                                                       \
    FLD_##ldk:                                                                                                         \
    {                                                                                                                  \
        auto args = M2rr::Decode(reader);                                                                              \
        LOG_INSTR;                                                                                                     \
        bool successful = interpreter.LoadFrame(Format::LoadAccessKind::LD_##ldk, args.rr.x, memspaceOffsetAcc);       \
        NEXT_COND(successful);                                                                                         \
    }
    FLD(U8)
    FLD(U16)
    FLD(32)
    FLD(S8)
    FLD(S16)
    FLD(F32)
    FLD(F64)
    FLD(64)
    FLD(S32TO64)
    FLD(REF)
#undef FLD

#define FST(stk)                                                                                                       \
    FST_##stk:                                                                                                         \
    {                                                                                                                  \
        auto args = M2rr::Decode(reader);                                                                              \
        LOG_INSTR;                                                                                                     \
        bool successful = interpreter.StoreFrame(Format::StoreAccessKind::ST_##stk, args.rr.x, memspaceOffsetAcc);     \
        NEXT_COND(successful);                                                                                         \
    }
    FST(8)
    FST(16)
    FST(32)
    FST(64)
    FST(REF)
    FST(F32)
    FST(F64)
#undef FST

#define FSTI(memSize, immSize, encoding)                                                                               \
    FSTI_##memSize##_##immSize:                                                                                        \
    {                                                                                                                  \
        auto args = encoding::Decode(reader);                                                                          \
        LOG_INSTR;                                                                                                     \
        uint64_t imm    = MathUtils::SignExtend(static_cast<uint64_t>(args.imm##immSize), immSize);                    \
        bool successful = interpreter.StoreFrameImm(Format::StoreAccessKind::ST_##memSize, imm, memspaceOffsetAcc);    \
        NEXT_COND(successful);                                                                                         \
    }
    FSTI(8, 8, M2i8)
    FSTI(16, 8, M2i8)
    FSTI(16, 16, M3i16)
    FSTI(32, 8, M2i8)
    FSTI(32, 16, M3i16)
    FSTI(32, 32, M5i32)
    FSTI(64, 8, M2i8)
    FSTI(64, 16, M3i16)
    FSTI(64, 32, M5i32)
    FSTI(64, 64, M9i64)
#undef FSTI

#undef MEM_NEXT
#undef NEXT
#undef NEXT_COND
}

void engine_log_int_start(DynamicFunctionHandle* handle, Ectype* ectype)
{
    auto& logger = Log::interpretation.Stream(Logging::Level::DEBUG);
    auto id      = handle->methodDef.GetFileId().id;
    auto offs    = handle->methodDef.GetOffset().value;
    logger.PrintFmt("Started interpretation of %p (%u;%u)", handle, id, offs);
    logger.NewLine();
}

void engine_log_int_end(DynamicFunctionHandle* handle, Ectype* ectype)
{
    auto& logger = Log::interpretation.Stream(Logging::Level::DEBUG);
    auto id      = handle->methodDef.GetFileId().id;
    auto offs    = handle->methodDef.GetOffset().value;
    logger.PrintFmt("Stopped interpretation of %p (%u;%u)", handle, id, offs);
    logger.NewLine();
    logger.Flush();
}
}

Thunk Interpretation::InterpretationLoop(
    Ectype* ectype, Frame frame, RTSupport::ThreadHandle handle, LiteralTable* literals, Decoder::ByteReader& reader0
) __attribute__((alias("engine_interpretation_loop")));

void Interpretation::InterpretationStart(DynamicFunctionHandle* handle, Ectype* ectype)
{
    engine_log_int_start(handle, ectype);
}

void Interpretation::InterpretationEnd(DynamicFunctionHandle* handle, Ectype* ectype)
{
    engine_log_int_end(handle, ectype);
}

RTSupport::TypeInfo Interpretation::builtinTypeInfos[BUILTIN_COUNT];
