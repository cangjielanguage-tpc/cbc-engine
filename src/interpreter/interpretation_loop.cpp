#include "interpretation_loop.h"
#include "cbc/formater_rt.h"
#include "cbc/frame.h"
#include "cbc/isa.h"
#include "cbc/isa_rt.h"
#include "engine/terms.h"
#include "interpreter.h"
#include "interpreter/code.h"
#include "interpreter/ectype.h"
#include "interpreter/implicit_exceptions.h"
#include "interpreter/loggers.h"
#include "interpreter/platform_traits.h"
#include "runtimesupport/adapters.h"
#include "runtimesupport/runtime.h"
#include "utils/assertion.h"
#include "utils/logger.h"
#include "utils/math.h"
#include "utils/ostream.h"
#include <atomic>

#include <cmath>
#include <cstdint>
#include <cstring>

using namespace Interpretation;
using namespace Cbc::RT;
using namespace RTSupport;

static void InitializeClosure(Ectype* ectype, bool instantiatedSret)
{
    struct ClosureObj {
        void* header;
        void* generic;
        void* instantiated;
    };

    /// CBC-provided closures always have two fields reserved for function pointers to
    /// generic and instantiated versions of the function. The generic version uses SRET
    /// because type-variable results are returned indirectly.
    auto closure = reinterpret_cast<ClosureObj*>(ectype->GetReference(IReg::IR1).value);

    closure->generic      = Adapters::GetDynCallTrampoline(0, true);
    closure->instantiated = Adapters::GetDynCallTrampoline(1, instantiatedSret);
}

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

#define NEXT_OR_THROW(successful, type)                                                                                \
    do {                                                                                                               \
        if (successful) {                                                                                              \
            NEXT;                                                                                                      \
        } else {                                                                                                       \
            THROW_IMPLICIT(type);                                                                                      \
        }                                                                                                              \
    } while (0)

#define THROW_EXPLICIT(exception)                                                                                      \
    do {                                                                                                               \
        uintptr_t exceptionObj = exception;                                                                            \
        auto func              = RTSupport::Execution::HandleException();                                              \
        reader0                = reader; /* save current pc */                                                         \
        return { func, reinterpret_cast<void*>(exceptionObj) };                                                        \
    } while (0)

#define THROW_IMPLICIT(type)                                                                                           \
    do {                                                                                                               \
        reader0 = reader;                                                                                              \
        return { RTSupport::Execution::ThrowImplicitException(), reinterpret_cast<void*>(type) };                      \
    } while (0)

#if defined(INT_SYMS) && defined(__APPLE__)
    #define LABEL(symbol_name)                                                                                         \
    symbol_name:                                                                                                       \
        __asm__ volatile(".globl _" #symbol_name "\n"                                                                  \
                         "_" #symbol_name ":\n" ::                                                                     \
                             :);
#elif defined(INT_SYMS)
    #define LABEL(symbol_name)                                                                                         \
    symbol_name:                                                                                                       \
        __asm__ volatile(".globl " #symbol_name "\n"                                                                   \
                         ".type " #symbol_name ", @function\n" #symbol_name ":\n" ::                                   \
                             :);
#else
    #define LABEL(symbol_name)                                                                                         \
    symbol_name:
#endif

#define CBC_RT_LABEL(opc, encoding, fmt) &&opc,
#define CBC_RT_MEM_LABEL(opc, encoding, fmt, tail) &&opc,
    static void* MAIN_TABLE[] = { CBC_RT_OPCODES(CBC_RT_LABEL) };

    static void* MEMSPACE_TABLE[] = { CBC_RT_MEMOPCODES(CBC_RT_MEM_LABEL) };

#ifdef NDEBUG
    #define LOG_INSTR
    #define SET_LABEL(instr_name) (void)(instr_name)
    #define DEBUG_INFO(instr_name) LABEL_SYMBOL(instr_name)
#else
    // Logging format is
    // [int] (stack depth) < (bc pos): instruction
    #define LOG_INSTR                                                                                                  \
        if (ectype->funcCtr > Log::skipThreshold) {                                                                    \
            Log::interpretation.Log(Logging::TRACE, [&](Stream::Output& logger) {                                      \
                logger.PrintFmt("#0x%lx < 0x%03lx: ", frame.start, pos - start);                                       \
                pos = reader.Cursor();                                                                                 \
                Cbc::RT::Log(literals, logger, args);                                                                  \
            });                                                                                                        \
        };
    // TODO: add ectype ptr as ID of thread.
    auto start   = reader.Start();
    auto pos     = reader.Cursor();
    auto& logger = Log::interpretation.Stream(Logging::Level::TRACE);
    __attribute__((used)) static volatile const char* label;
#endif

    uint64_t memspaceOffsetAcc = 0;

    // jump to the instruction handler.
    NEXT;

    // clang-format off
    // -- Main opcode table --
LABEL(HALT) {
    FATAL("halt");
    return {};
}
LABEL(LOG) {
    auto args   = B9i64::Decode(reader);
    auto string = (char*)args.imm64.ptr;
    Log::stream << string << Stream::endl;
    NEXT;
}
LABEL(RET) {
    auto args = B1::Decode(reader);
    LOG_INSTR;
    return {};
}
LABEL(NOP) {
    auto args = B1::Decode(reader);
    LOG_INSTR;
    NEXT;
}
LABEL(MOV) {
    auto args = B2rr::Decode(reader);
    LOG_INSTR;
    interpreter.Mov(args.rr.x.IR(), args.rr.y.IR());
    NEXT;
}
LABEL(MOVI) {
    auto args = B2xr::Decode(reader);
    LOG_INSTR;
    interpreter.MovI(args.xr.r.IR(), MathUtils::SignExtend(static_cast<uint64_t>(args.xr.imm), 4));
    NEXT;
}
LABEL(FMOV) {
    auto args = B2rr::Decode(reader);
    LOG_INSTR;
    interpreter.Mov(args.rr.x.FR(), args.rr.y.FR());
    NEXT;
}
LABEL(MOVI2F) {
    auto args = B2rr::Decode(reader);
    LOG_INSTR;
    interpreter.Mov(args.rr.x.FR(), args.rr.y.IR());
    NEXT;
}
LABEL(MOVF2I) {
    auto args = B2rr::Decode(reader);
    LOG_INSTR;
    interpreter.Mov(args.rr.x.IR(), args.rr.y.FR());
    NEXT;
}
LABEL(GC_POINT) {
    auto args = B1::Decode(reader);
    LOG_INSTR;
    bool is_sp = RTSupport::Execution::IsPendingSafePoint();
    if (!is_sp) {
        NEXT;
    }

    reader0 = reader;

    return { RTSupport::Execution::GcPointTrampoline(), RTSupport::Execution::GcPoint() };
}
LABEL(FMOVI32) {
    auto args = B6xri32::Decode(reader);
    LOG_INSTR;
    interpreter.MovI(args.xr.r.FR(), args.imm32.fimm);
    NEXT;
}
LABEL(FMOVI64) {
    auto args = B10xri64::Decode(reader);
    LOG_INSTR;
    interpreter.MovI(args.xr.r.FR(), args.imm64.dimm);
    NEXT;
}
LABEL(BCC32I) {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    int64_t delta = interpreter.template Bcc<ImmKind::VALUE, Width::W32>(
        args.xi12.imm4.CC(), args.rr.x, args.rr.y, args.xi12.imm12
    );
    JUMP;
}
LABEL(BCC32L) {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    int64_t delta = interpreter.template Bcc<ImmKind::LITERAL, Width::W32>(
        args.xi12.imm4.CC(), args.rr.x, args.rr.y, args.xi12.imm12
    );
    JUMP;
}
LABEL(BCC64I) {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    int64_t delta = interpreter.template Bcc<ImmKind::VALUE, Width::W64>(
        args.xi12.imm4.CC(), args.rr.x, args.rr.y, args.xi12.imm12
    );
    JUMP;
}
LABEL(BCC64L) {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    int64_t delta = interpreter.template Bcc<ImmKind::LITERAL, Width::W64>(
        args.xi12.imm4.CC(), args.rr.x, args.rr.y, args.xi12.imm12
    );
    JUMP;
}
LABEL(BCCI32I) {
    auto args = B5xi12ri12::Decode(reader);
    LOG_INSTR;
    int64_t delta = interpreter.template BccImm<ImmKind::VALUE, ImmKind::VALUE, Width::W32>(
        args.xi12.imm4.CC(), args.ri12.r.IR(), args.ri12.imm12, args.xi12.imm12
    );
    JUMP;
}
LABEL(BCCI64I) {
    auto args = B5xi12ri12::Decode(reader);
    LOG_INSTR;
    int64_t delta = interpreter.template BccImm<ImmKind::VALUE, ImmKind::VALUE, Width::W64>(
        args.xi12.imm4.CC(), args.ri12.r.IR(), args.ri12.imm12, args.xi12.imm12
    );
    JUMP;
}
LABEL(BCCI32L) {
    auto args = B5xi12ri12::Decode(reader);
    LOG_INSTR;
    int64_t delta = interpreter.template BccImm<ImmKind::VALUE, ImmKind::LITERAL, Width::W32>(
        args.xi12.imm4.CC(), args.ri12.r.IR(), args.ri12.imm12, args.xi12.imm12
    );
    JUMP;
}
LABEL(BCCI64L) {
    auto args = B5xi12ri12::Decode(reader);
    LOG_INSTR;
    int64_t delta = interpreter.template BccImm<ImmKind::VALUE, ImmKind::LITERAL, Width::W64>(
        args.xi12.imm4.CC(), args.ri12.r.IR(), args.ri12.imm12, args.xi12.imm12
    );
    JUMP;
}
LABEL(BCCL32I) {
    auto args = B5xi12ri12::Decode(reader);
    LOG_INSTR;
    int64_t delta = interpreter.template BccImm<ImmKind::LITERAL, ImmKind::VALUE, Width::W32>(
        args.xi12.imm4.CC(), args.ri12.r.IR(), args.ri12.imm12, args.xi12.imm12
    );
    JUMP;
}
LABEL(BCCL64I) {
    auto args = B5xi12ri12::Decode(reader);
    LOG_INSTR;
    int64_t delta = interpreter.template BccImm<ImmKind::LITERAL, ImmKind::VALUE, Width::W64>(
        args.xi12.imm4.CC(), args.ri12.r.IR(), args.ri12.imm12, args.xi12.imm12
    );
    JUMP;
}
LABEL(BCCL32L) {
    auto args = B5xi12ri12::Decode(reader);
    LOG_INSTR;
    int64_t delta = interpreter.template BccImm<ImmKind::LITERAL, ImmKind::LITERAL, Width::W32>(
        args.xi12.imm4.CC(), args.ri12.r.IR(), args.ri12.imm12, args.xi12.imm12
    );
    JUMP;
}
LABEL(BCCL64L) {
    auto args = B5xi12ri12::Decode(reader);
    LOG_INSTR;
    int64_t delta = interpreter.template BccImm<ImmKind::LITERAL, ImmKind::LITERAL, Width::W64>(
        args.xi12.imm4.CC(), args.ri12.r.IR(), args.ri12.imm12, args.xi12.imm12
    );
    JUMP;
}
LABEL(BRANCH_IS_REF) {
    auto args = B3xi12::Decode(reader);
    LOG_INSTR;
    auto tiReg    = IReg::From(args.xi12.imm4);
    auto ti       = TypeInfo(ectype->GetPrimitive(tiReg).u64);
    int64_t delta = 0;
    if (RTSupport::Execution::IsReference(ti)) {
        uint16_t value = args.xi12.imm12;
        delta          = MathUtils::SignExtend<int64_t>(value, 12);
    }
    JUMP;
}
LABEL(JMP32) {
    auto args = B5i32::Decode(reader);
    LOG_INSTR;
    int64_t delta = interpreter.Jmp(args.imm32.imm);
    JUMP;
}
LABEL(BIN32) {
    auto args = B3xrrr::Decode(reader);
    LOG_INSTR;
    bool successful =
        interpreter.template Binary<Width::W32>(args.xr.imm.Common(), args.xr.r.IR(), args.rr.x.IR(), args.rr.y.IR());
    NEXT_COND(successful);
}
LABEL(BIN64) {
    auto args = B3xrrr::Decode(reader);
    LOG_INSTR;
    bool successful =
        interpreter.template Binary<Width::W64>(args.xr.imm.Common(), args.xr.r.IR(), args.rr.x.IR(), args.rr.y.IR());
    NEXT_COND(successful);
}
LABEL(BINI32I) {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.template BinaryImm<ImmKind::VALUE, Width::W32>(
        args.xi12.imm4.Common(), args.rr.x.IR(), args.rr.y.IR(), args.xi12.imm12
    );
    NEXT_COND(successful);
}
LABEL(BINI64I) {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.template BinaryImm<ImmKind::VALUE, Width::W64>(
        args.xi12.imm4.Common(), args.rr.x.IR(), args.rr.y.IR(), args.xi12.imm12
    );
    NEXT_COND(successful);
}
LABEL(BINI32L) {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.template BinaryImm<ImmKind::LITERAL, Width::W32>(
        args.xi12.imm4.Common(), args.rr.x.IR(), args.rr.y.IR(), args.xi12.imm12
    );
    NEXT_COND(successful);
}
LABEL(BINI64L) {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.template BinaryImm<ImmKind::LITERAL, Width::W64>(
        args.xi12.imm4.Common(), args.rr.x.IR(), args.rr.y.IR(), args.xi12.imm12
    );
    NEXT_COND(successful);
}
LABEL(CBIN8) {
    auto args = B3xrrr::Decode(reader);
    LOG_INSTR;
    bool successful =
        interpreter.template Binary<Width::W8>(args.xr.imm.Checked(), args.xr.r.IR(), args.rr.x.IR(), args.rr.y.IR());
    NEXT_OR_THROW(successful, Type::OverflowException);
}
LABEL(CBIN16) {
    auto args = B3xrrr::Decode(reader);
    LOG_INSTR;
    bool successful =
        interpreter.template Binary<Width::W16>(args.xr.imm.Checked(), args.xr.r.IR(), args.rr.x.IR(), args.rr.y.IR());
    NEXT_OR_THROW(successful, Type::OverflowException);
}
LABEL(CBIN32) {
    auto args = B3xrrr::Decode(reader);
    LOG_INSTR;
    bool successful =
        interpreter.template Binary<Width::W32>(args.xr.imm.Checked(), args.xr.r.IR(), args.rr.x.IR(), args.rr.y.IR());
    NEXT_OR_THROW(successful, Type::OverflowException);
}
LABEL(CBIN64) {
    auto args = B3xrrr::Decode(reader);
    LOG_INSTR;
    bool successful =
        interpreter.template Binary<Width::W64>(args.xr.imm.Checked(), args.xr.r.IR(), args.rr.x.IR(), args.rr.y.IR());
    NEXT_OR_THROW(successful, Type::OverflowException);
}
LABEL(CBINI8I) {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.template BinaryImm<Width::W8>(
        args.xi12.imm4.Checked(), args.rr.x.IR(), args.rr.y.IR(), MathUtils::SignExtend(static_cast<uint64_t>(args.xi12.imm12), 12)
    );
    NEXT_OR_THROW(successful, Type::OverflowException);
}
LABEL(CBINI16I) {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.template BinaryImm<Width::W16>(
        args.xi12.imm4.Checked(), args.rr.x.IR(), args.rr.y.IR(), MathUtils::SignExtend(static_cast<uint64_t>(args.xi12.imm12), 12)
    );
    NEXT_OR_THROW(successful, Type::OverflowException);
}
LABEL(CBINI32I) {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.template BinaryImm<Width::W32>(
        args.xi12.imm4.Checked(), args.rr.x.IR(), args.rr.y.IR(), MathUtils::SignExtend(static_cast<uint64_t>(args.xi12.imm12), 12)
    );
    NEXT_OR_THROW(successful, Type::OverflowException);
}
LABEL(CBINI64I) {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.template BinaryImm<Width::W64>(
        args.xi12.imm4.Checked(), args.rr.x.IR(), args.rr.y.IR(), MathUtils::SignExtend(static_cast<uint64_t>(args.xi12.imm12), 12)
    );
    NEXT_OR_THROW(successful, Type::OverflowException);
}
LABEL(CBINI8W) {
    auto args = BinaryChecked::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.template BinaryImm<Width::W8>(args.op, args.rr.x.IR(), args.rr.y.IR(), args.imm.imm);
    NEXT_OR_THROW(successful, Type::OverflowException);
}
LABEL(CBINI16W) {
    auto args = BinaryChecked::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.template BinaryImm<Width::W16>(args.op, args.rr.x.IR(), args.rr.y.IR(), args.imm.imm);
    NEXT_OR_THROW(successful, Type::OverflowException);
}
LABEL(CBINI32W) {
    auto args = BinaryChecked::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.template BinaryImm<Width::W32>(args.op, args.rr.x.IR(), args.rr.y.IR(), args.imm.imm);
    NEXT_OR_THROW(successful, Type::OverflowException);
}
LABEL(CBINI64W) {
    auto args = BinaryChecked::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.template BinaryImm<Width::W64>(args.op, args.rr.x.IR(), args.rr.y.IR(), args.imm.imm);
    NEXT_OR_THROW(successful, Type::OverflowException);
}
LABEL(FBIN32) {
    auto args = B3xrrr::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.template Binary<Width::W32>(
        args.xr.imm.FloatOperations(), args.xr.r.FR(), args.rr.x.FR(), args.rr.y.FR()
    );
    NEXT_COND(successful);
}
LABEL(FBIN64) {
    auto args = B3xrrr::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.template Binary<Width::W64>(
        args.xr.imm.FloatOperations(), args.xr.r.FR(), args.rr.x.FR(), args.rr.y.FR()
    );
    NEXT_COND(successful);
}
LABEL(FUN32) {
    auto args = B3xrrr::Decode(reader);
    LOG_INSTR;
    bool successful =
        interpreter.template Unary<Width::W32>(args.xr.imm.FloatOperations(), args.xr.r.FR(), args.rr.y.FR());
    NEXT_COND(successful);
}
LABEL(FUN64) {
    auto args = B3xrrr::Decode(reader);
    LOG_INSTR;
    bool successful =
        interpreter.template Unary<Width::W64>(args.xr.imm.FloatOperations(), args.xr.r.FR(), args.rr.y.FR());
    NEXT_COND(successful);
}
LABEL(FMATHUN32) {
    auto args = B3xrrr::Decode(reader);
    LOG_INSTR;
    bool successful =
        interpreter.template FMathUnary<Width::W32>(args.xr.imm.FloatMathOp(), args.xr.r.FR(), args.rr.y.FR());
    NEXT_COND(successful);
}
LABEL(FMATHUN64) {
    auto args = B3xrrr::Decode(reader);
    LOG_INSTR;
    bool successful =
        interpreter.template FMathUnary<Width::W64>(args.xr.imm.FloatMathOp(), args.xr.r.FR(), args.rr.y.FR());
    NEXT_COND(successful);
}
LABEL(NEWOBJ_ACC_G) {
    auto args = B2rr::Decode(reader);
    LOG_INSTR;
    auto tiReg = args.rr.x;
    auto type  = TypeInfo(ectype->GetPrimitive(tiReg.IR()).u64);

    // Puts result to `IR_ACC`.
    auto func = Execution::AllocateObjectInstanceAcc();

    reader0 = reader; // save current pc

    return { func, type.Raw() };
}
LABEL(NEWOBJ_G) {
    auto args = B2rr::Decode(reader);
    LOG_INSTR;
    auto tiReg = args.rr.x;
    auto type  = TypeInfo(ectype->GetPrimitive(tiReg.IR()).u64);

    // Puts result to `IR1`.
    auto func = Execution::AllocateObjectInstance();

    reader0 = reader; // save current pc

    return { func, type.Raw() };
}
LABEL(NEWOBJ_PINNED_G) {
    auto args = B2rr::Decode(reader);
    LOG_INSTR;
    auto tiReg = args.rr.x;
    auto type  = TypeInfo(ectype->GetPrimitive(tiReg.IR()).u64);

    // Puts result to `IR1`.
    auto func = Execution::AllocateObjectPinnedInstance();

    reader0 = reader; // save current pc

    return { func, type.Raw() };
}
LABEL(NEWOBJ) {
    auto args = B9i64::Decode(reader);
    LOG_INSTR;
    auto type = TypeInfo(static_cast<uintptr_t>(args.imm64.imm));

    // Puts result to `IR1`.
    auto func = Execution::AllocateObjectInstance();

    reader0 = reader; // save current pc

    return { func, type.Raw() };
}
LABEL(NEWOBJ_PINNED) {
    auto args = B9i64::Decode(reader);
    LOG_INSTR;
    auto type = TypeInfo(static_cast<uintptr_t>(args.imm64.imm));

    // Puts result to `IR1`.
    auto func = Execution::AllocateObjectPinnedInstance();

    reader0 = reader; // save current pc

    return { func, type.Raw() };
}
LABEL(NEWBOX) {
    auto args = B2xr::Decode(reader);
    LOG_INSTR;
    auto btype = builtinTypeInfos[args.xr.imm];

    // Puts result to `IR1`.
    auto func = Execution::AllocateObjectInstanceAcc();

    reader0 = reader; // save current pc

    return { func, btype.Raw() };
}
LABEL(NEWBOX2) {
    auto args = B9i64::Decode(reader);
    LOG_INSTR;
    auto type = TypeInfo(static_cast<uintptr_t>(args.imm64.imm));

    // Puts result to `IR1`.
    auto func = RTSupport::Execution::AllocateObjectInstanceAcc();

    reader0 = reader; // save current pc

    return { func, type.Raw() };
}
LABEL(READ_STRUCT_FIELD) {
    auto args = StructFieldOp::Decode(reader);
    LOG_INSTR;
    auto dst   = ectype->GetPrimitive(args.rr.x.IR()).u64;
    auto base  = ectype->GetReference(args.rr.y.IR());
    auto field = ectype->GetPrimitive(args.field.x.IR()).u64;
    RTSupport::Execution::ReadStructField(dst, base, field, args.ti, handle);
    NEXT;
}
LABEL(WRITE_STRUCT_FIELD) {
    auto args = StructFieldOp::Decode(reader);
    LOG_INSTR;
    auto src   = ectype->GetPrimitive(args.rr.x.IR()).u64;
    auto base  = ectype->GetReference(args.rr.y.IR());
    auto field = ectype->GetPrimitive(args.field.x.IR()).u64;
    RTSupport::Execution::WriteStructField(src, base, field, args.ti, handle);
    NEXT;
}
LABEL(INITCLOSURE) {
    auto args = B1::Decode(reader);
    LOG_INSTR;
    InitializeClosure(ectype, false);
    NEXT;
}
LABEL(INITCLOSURE_SRET) {
    auto args = B1::Decode(reader);
    LOG_INSTR;
    InitializeClosure(ectype, true);
    NEXT;
}
LABEL(SPAWN_FUTURE) {
    auto args = B1::Decode(reader);
    LOG_INSTR;

    auto func = RTSupport::Execution::SpawnFuture();

    reader0 = reader; // save current pc

    return { func, func };
}
LABEL(SPAWN) {
    auto args = B9i64::Decode(reader);
    LOG_INSTR;
    auto type = TypeInfo(static_cast<uintptr_t>(args.imm64.imm));

    // TODO: it seems that spawn could be called directly
    auto func = RTSupport::Execution::Spawn();

    reader0 = reader; // save current pc

    return { func, type.Raw() };
}
LABEL(NEWARR) {
    auto args = B9i64::Decode(reader);
    LOG_INSTR;
    auto type = TypeInfo(static_cast<uintptr_t>(args.imm64.imm));

    // Puts result to `IR1`, expects length to be passed on `IR2`.
    auto func = RTSupport::Execution::AllocateArrayInstance();

    reader0 = reader; // save current pc

    return { func, type.Raw() };
}
LABEL(LOAD_ADDR) {
    auto args = B2xr::Decode(reader);
    LOG_INSTR;
    auto location   = literals->at(reader.Read16()).u64;
    bool successful = interpreter.LoadAddr(args.xr.imm.LDK(), args.xr.r, location);
    NEXT_COND(successful);
}
LABEL(STORE_ADDR) {
    auto args = B2xr::Decode(reader);
    LOG_INSTR;
    auto location   = literals->at(reader.Read16()).u64;
    bool successful = interpreter.StoreAddr(args.xr.imm.STK(), args.xr.r, location);
    NEXT_COND(successful);
}
LABEL(LOAD_OBJ_F)
LABEL(LOAD_OBJ) {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.LoadObj(args.xi12.imm4.LDK(), args.rr.x, args.rr.y.IR(), args.xi12.imm12);
    NEXT_COND(successful);
}
LABEL(STORE_OBJ_F)
LABEL(STORE_OBJ) {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.StoreObj(args.xi12.imm4.STK(), args.rr.x, args.rr.y.IR(), args.xi12.imm12);
    NEXT_COND(successful);
}
LABEL(LOAD_ARR_F)
LABEL(LOAD_ARR) {
    auto args = B3xrrr::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.LoadArray(args.xr.imm.LDK(), args.xr.r, args.rr.x.IR(), args.rr.y.IR());
    NEXT_COND(successful);
}
LABEL(STORE_ARR_F)
LABEL(STORE_ARR) {
    auto args = B3xrrr::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.StoreArray(args.xr.imm.STK(), args.xr.r, args.rr.x.IR(), args.rr.y.IR());
    NEXT_COND(successful);
}
LABEL(LOAD_REC_F)
LABEL(LOAD_REC) {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.LoadRec(args.xi12.imm4.LDK(), args.rr.x, args.rr.y.IR(), args.xi12.imm12);
    NEXT_COND(successful);
}
LABEL(STORE_REC_F)
LABEL(STORE_REC) {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.StoreRec(args.xi12.imm4.STK(), args.rr.x, args.rr.y.IR(), args.xi12.imm12);
    NEXT_COND(successful);
}

LABEL(COPY_DERIVED) {
    auto args = CopyDerived::Decode(reader);
    LOG_INSTR;
    interpreter.CopyDerived(args.dst.x.IR(), args.dst.y.IR(), args.src.x.IR(), args.src.y.IR(), args.ti);
    NEXT;
}

// Unused. TODO: support copying in generic context
LABEL(COPY_DERIVED_GENERIC) {
    auto args = CopyDerivedGeneric::Decode(reader);
    LOG_INSTR;
    auto tiReg = args.ti.x;
    auto ti = TypeInfo(ectype->GetPrimitive(tiReg.IR()).u64);
    interpreter.CopyDerived(args.dst.x.IR(), args.dst.y.IR(), args.src.x.IR(), args.src.y.IR(), ti);
    NEXT;
}

LABEL(INDEX) {
    auto args = Index::Decode(reader);
    LOG_INSTR;
    interpreter.LeaIndex(args.rr.x.IR(), args.rr.y.IR(), args.idx.r.IR(), args.ti, args.idx.imm);
    NEXT;
}

// Unused. TODO: support element access for generic arrays
LABEL(INDEX_GENERIC) {
    auto args = IndexGeneric::Decode(reader);
    LOG_INSTR;
    auto tiReg = args.idx.y;
    auto ti = TypeInfo(ectype->GetPrimitive(tiReg.IR()).u64);
    interpreter.LeaIndex(args.rr.x.IR(), args.rr.y.IR(), args.idx.x.IR(), ti);
    NEXT;
}

LABEL(LOAD_FRAME_F)
LABEL(LOAD_FRAME) {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.LoadFrame(args.xi12.imm4.LDK(), args.rr.x, args.xi12.imm12);
    NEXT_COND(successful);
}
LABEL(STORE_FRAME_F)
LABEL(STORE_FRAME) {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.StoreFrame(args.xi12.imm4.STK(), args.rr.x, args.xi12.imm12);
    NEXT_COND(successful);
}
LABEL(LOAD_LONG_FRAME_F)
LABEL(LOAD_LONG_FRAME) {
    auto args = B7xrrri32::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.LoadFrame(args.xr.imm.LDK(), args.xr.r, args.imm32.imm);
    NEXT_COND(successful);
}
LABEL(STORE_LONG_FRAME_F)
LABEL(STORE_LONG_FRAME) {
    auto args = B7xrrri32::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.StoreFrame(args.xr.imm.STK(), args.xr.r, args.imm32.imm);
    NEXT_COND(successful);
}
LABEL(LOAD_LONG_REC_F)
LABEL(LOAD_LONG_REC) {
    auto args = B7xrrri32::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.LoadRec(args.xr.imm.LDK(), args.xr.r, args.rr.y.IR(), args.imm32.imm);
    NEXT_COND(successful);
}
LABEL(STORE_LONG_REC_F)
LABEL(STORE_LONG_REC) {
    auto args = B7xrrri32::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.StoreRec(args.xr.imm.STK(), args.xr.r, args.rr.y.IR(), args.imm32.imm);
    NEXT_COND(successful);
}
LABEL(LOAD_LONG_DERIVED_F)
LABEL(LOAD_LONG_DERIVED) {
    auto args = B7xrrri32::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.LoadDerived(
        args.xr.imm.LDK(), args.xr.r, args.rr.x.IR(), args.rr.y.IR(), args.imm32.imm);
    NEXT_COND(successful);
}
LABEL(STORE_LONG_DERIVED_F)
LABEL(STORE_LONG_DERIVED) {
    auto args = B7xrrri32::Decode(reader);
    LOG_INSTR;
    bool successful = interpreter.StoreDerived(
        args.xr.imm.STK(), args.xr.r, args.rr.x.IR(), args.rr.y.IR(), args.imm32.imm);
    NEXT_COND(successful);
}
LABEL(LOAD_GENERIC) {
    auto args = B3rrrr::Decode(reader);
    LOG_INSTR;
    // TODO: reorder args, so it would require less bit-shifting
    auto dstReg     = args.rr1.x.IR();
    auto baseReg    = args.rr1.y.IR();
    auto derivedReg = args.rr2.x.IR();
    auto tiReg      = args.rr2.y.IR();

    auto derived  = ectype->GetReference(derivedReg);
    auto typeInfo = TypeInfo(ectype->GetPrimitive(tiReg).u64);
    if (RTSupport::Execution::IsReference(typeInfo)) {
        auto base = ectype->GetReference(baseReg);
        auto obj  = RTSupport::Execution::ReadObjectInstance(base, derived.value, handle);
        ectype->Put(dstReg, obj);
        NEXT;
    } else {
        // The operation require two steps: box allocation and ReadGeneric invocation.
        // Because box allocation can provoke GC or throw, we should not perform it with C++ frame on the stack.
        ectype->Put(IReg::IR_ACC, Value::Reference { derived.value });

        // on x64 and aarch64 pointers are 48-bit values
        uint64_t rawTi  = typeInfo.UInt();
        uint64_t packed = 0ULL | dstReg | (baseReg << 4) | (IReg::IR_ACC << 8) | rawTi << 12;
        reader0         = reader;
        return { .function = RTSupport::Execution::LoadGeneric(), .argUInt = packed };
    }
}
LABEL(STORE_GENERIC) {
    auto args = B3rrrr::Decode(reader);
    LOG_INSTR;
    auto srcReg     = args.rr1.x.IR();
    auto baseReg    = args.rr1.y.IR();
    auto derivedReg = args.rr2.x.IR();
    auto tiReg      = args.rr2.y.IR();

    auto base    = ectype->GetReference(baseReg);
    auto derived = ectype->GetReference(derivedReg);
    auto obj     = ectype->GetReference(srcReg);

    auto typeInfo = TypeInfo(ectype->GetPrimitive(tiReg).u64);
    if (RTSupport::Execution::IsReference(typeInfo)) {
        RTSupport::Execution::WriteObjectInstance(base, derived.value, obj, handle);
        NEXT;
    } else {
        uint32_t size = RTSupport::MetaInfo::GetTypeSize(typeInfo);
        RTSupport::Execution::WriteGeneric(base, derived.value, obj, size, handle);
        NEXT;
    }
}
LABEL(LEA_GENERIC) {
    auto args = B7xrrri32::Decode(reader);
    LOG_INSTR;
    auto dst           = args.xr.r.IR();
    auto base          = ectype->GetReference(args.rr.x.IR()).value;
    auto ti            = TypeInfo(ectype->GetPrimitive(args.rr.y.IR()).u64);
    auto offs          = RTSupport::Execution::GetFieldOffset(ti, args.imm32.imm, false);
    ectype->Put(dst, Value::Reference { .value = base + offs });
    NEXT;
}
LABEL(PREP_TYPED) {
    auto args = B13i64i32::Decode(reader);
    LOG_INSTR;
    auto typedOffset = args.imm32.imm;
    auto typeInfo    = TypeInfo(static_cast<uintptr_t>(args.imm64.imm));
    typeInfo.VisitReferenceOffsets([&](uint32_t offset) {
        interpreter.StoreFrameImm(StoreAccessKind::ST_64, 0, typedOffset + offset);
    });
    NEXT;
}
LABEL(SCC32) {
    auto args = B3xrrr::Decode(reader);
    LOG_INSTR;
    interpreter.template SCC<Width::W32>(args.xr.imm.CC(), args.xr.r.IR(), args.rr.x.IR(), args.rr.y.IR());
    NEXT;
}
LABEL(SCC64) {
    auto args = B3xrrr::Decode(reader);
    LOG_INSTR;
    interpreter.template SCC<Width::W64>(args.xr.imm.CC(), args.xr.r.IR(), args.rr.x.IR(), args.rr.y.IR());
    NEXT;
}
LABEL(FSCC32) {
    auto args = B3xrrr::Decode(reader);
    LOG_INSTR;
    interpreter.template SCC<Width::W32>(args.xr.imm.CC(), args.xr.r.IR(), args.rr.x.FR(), args.rr.y.FR());
    NEXT;
}
LABEL(FSCC64) {
    auto args = B3xrrr::Decode(reader);
    LOG_INSTR;
    interpreter.template SCC<Width::W64>(args.xr.imm.CC(), args.xr.r.IR(), args.rr.x.FR(), args.rr.y.FR());
    NEXT;
}
LABEL(SCCI32I) {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    interpreter.template SCCImm<ImmKind::VALUE, Width::W32>(
        args.xi12.imm4.CC(), args.rr.x.IR(), args.rr.y.IR(), args.xi12.imm12
    );
    NEXT;
}
LABEL(SCCI64I) {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    interpreter.template SCCImm<ImmKind::VALUE, Width::W64>(
        args.xi12.imm4.CC(), args.rr.x.IR(), args.rr.y.IR(), args.xi12.imm12
    );
    NEXT;
}
LABEL(SCCI32L) {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    interpreter.template SCCImm<ImmKind::LITERAL, Width::W32>(
        args.xi12.imm4.CC(), args.rr.x.IR(), args.rr.y.IR(), args.xi12.imm12
    );
    NEXT;
}
LABEL(SCCI64L) {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    interpreter.template SCCImm<ImmKind::LITERAL, Width::W64>(
        args.xi12.imm4.CC(), args.rr.x.IR(), args.rr.y.IR(), args.xi12.imm12
    );
    NEXT;
}
LABEL(OFFSET) {
    auto args = Offset::Decode(reader);
    LOG_INSTR;
    auto dst  = args.rr.x.IR();
    auto ti   = TypeInfo(ectype->GetPrimitive(args.rr.y.IR()).u64);
    auto offs = RTSupport::Execution::GetFieldOffset(ti, args.idx, false);
    ectype->Put(dst, Value::Primitive { offs });
    NEXT;
}
LABEL(TYPE_ARG) {
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
LABEL(CONVERT) {
    auto args = B3xxrr::Decode(reader);
    LOG_INSTR;
    interpreter.Convert(args.xx.imm1.ConvertType(), args.xx.imm2.ConvertType(), args.rr.x, args.rr.y);
    NEXT;
}
LABEL(BFXS) {
    auto args = BFX::Decode(reader);
    LOG_INSTR;
    interpreter.BitFieldExtract(args.rr.x, args.rr.y, args.offs, args.size, true);
    NEXT;
}
LABEL(BFXZ) {
    auto args = BFX::Decode(reader);
    LOG_INSTR;
    interpreter.BitFieldExtract(args.rr.x, args.rr.y, args.offs, args.size, false);
    NEXT;
}
LABEL(DIRECT_CALL_2I) {
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
LABEL(DIRECT_CALL_2C) {
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
LABEL(CALL_CLOSURE_SRET) {
    if constexpr (HAS_SRET_SHIFT) {
        auto args = B1::Decode(reader);
        LOG_INSTR;
        auto reference = ectype->GetReference(IReg::IR2);
        reader0        = reader;
        return Execution::GetClosureThunk(reference, true);
    }
    // fallthrough
}
LABEL(CALL_CLOSURE) {
    auto args = B1::Decode(reader);
    LOG_INSTR;
    auto reference = ectype->GetReference(IReg::IR1);
    reader0        = reader;
    return Execution::GetClosureThunk(reference, true);
}
LABEL(CALL_CLOSURE_GENERIC) {
    auto args = B1::Decode(reader);
    LOG_INSTR;
    auto receiver = IReg::IR1;
    if (HAS_SRET_SHIFT) { // generic closure calls are always considered as `sret`
        receiver = Cbc::IReg::IR2;
    }
    auto reference = ectype->GetReference(receiver);
    reader0        = reader;
    return Execution::GetClosureThunk(reference, false);
}
LABEL(VIRTUAL_CALL) {
    auto args = VirtualCall::Decode(reader);
    LOG_INSTR;
    auto vnum      = args.vnum;
    auto extDefNum = args.edef;

    auto receiver = IReg::IR1;
    if (HAS_SRET_SHIFT && args.sret) {
        receiver = Cbc::IReg::IR2;
    }
    auto reference = ectype->GetReference(receiver);

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

LABEL(INTERFACE_CALL) {
    auto args = InterfaceCall::Decode(reader);
    LOG_INSTR;
    auto num       = args.vnum;
    auto interf    = TypeInfo(static_cast<uintptr_t>(args.ti));

    auto receiver = IReg::IR1;
    if (HAS_SRET_SHIFT && args.sret) {
        receiver = Cbc::IReg::IR2;
    }
    auto reference = ectype->GetReference(receiver);

    struct Object {
        TypeInfo header;
    };

    Object* object = reinterpret_cast<Object*>(reference.value);
    auto typeInfo  = object->header;

    // For proper support of fibers, the following call MUST drop the current frame.
    // This can not be guaranteed by C++ compiler consistently, because TCO
    // is not guaranteed and `mustcall` attribute is not supported
    // fully by gcc/clang compilers.
    //
    // Instead, the following call will drop the current frame manually
    // (outside of unit-test framework).

    reader0 = reader; // save current pc

    return Execution::GetInterfaceThunk(typeInfo, interf, num);
}
LABEL(INTERFACE_CALL_GENERIC) {
    auto args = InterfaceCallGeneric::Decode(reader);
    LOG_INSTR;
    auto num    = args.vnum;
    auto sret   = args.sret;
    auto interf = TypeInfo(ectype->GetPrimitive(IReg::IR_ACC).u64);

    auto receiver = IReg::IR1;
    if (HAS_SRET_SHIFT && sret) {
        receiver = Cbc::IReg::IR2;
    }
    auto reference = ectype->GetReference(receiver);

    struct Object {
        TypeInfo header;
    };

    Object* object = reinterpret_cast<Object*>(reference.value);
    auto typeInfo  = object->header;

    auto outerTI = Execution::GetMethodOuterTi(typeInfo, interf, num);

    if (args.argn < IReg::VIRT_COUNT) {
        ectype->Put(IReg::From(args.argn), Value::Primitive { outerTI.UInt() });
    } else {
        // FIXME: share the same offset calculation logic as in rewriter.
        auto untypedSlot = args.argn - IReg::VIRT_COUNT;
        auto slotOffset  = untypedSlot * STACK_SLOT_SIZE;
        auto location    = reinterpret_cast<TypeInfo*>(frame.start + slotOffset);
        *location        = outerTI;
    }

    // For proper support of fibers, the following call MUST drop the current frame.
    // This can not be guaranteed by C++ compiler consistently, because TCO
    // is not guaranteed and `mustcall` attribute is not supported
    // fully by gcc/clang compilers.
    //
    // Instead, the following call will drop the current frame manually
    // (outside of unit-test framework).

    reader0 = reader; // save current pc

    return Execution::GetInterfaceThunk(typeInfo, interf, num);
}

LABEL(ASSIGN_GENERIC) {
    auto args = B3xrrr::Decode(reader);
    auto rdst = args.xr.r.IR();
    auto rsrc = args.rr.x.IR();
    auto rti  = args.rr.y.IR();

    auto src = ectype->GetReference(rsrc);
    auto dst = ectype->GetReference(rdst);
    auto ti  = TypeInfo(ectype->GetPrimitive(rti).u64);

    Execution::WriteStructField(src.value + MetaInfo::ObjectHeaderSize(), dst, dst.value + MetaInfo::ObjectHeaderSize(), ti, handle);
    NEXT;
}

LABEL(IOF_GENERIC) {
    auto args = B3xrrr::Decode(reader);
    auto dst  = args.xr.r.IR();
    auto robj = args.rr.x.IR();
    auto rti  = args.rr.y.IR();

    auto obj = ectype->GetReference(robj);
    auto ti  = TypeInfo(ectype->GetPrimitive(rti).u64);
    ectype->Put(dst, Value::Primitive { .u64 = Execution::IsInstanceOf(obj, ti) });
    NEXT;
}

LABEL(STRING_INIT) {
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

LABEL(NULLCHECK) {
    auto args = B2xr::Decode(reader);
    LOG_INSTR;
    auto ref = ectype->GetReference(args.xr.r.IR());
    NEXT_OR_THROW(ref.value != 0, Type::NoneValueException);
}

LABEL(DIVCHECK) {
    auto args = B2xr::Decode(reader);
    LOG_INSTR;
    auto div = ectype->GetPrimitive(args.xr.r.IR());
    NEXT_OR_THROW(div.u64 != 0, Type::ArithmeticException);
}

LABEL(LOAD_GENERIC_TI) {
    auto args = B9i64::Decode(reader);
    LOG_INSTR;
    auto termValue = args.imm64.imm;

    // Raw reinterpetation of 64 bit value.
    Engine::Term term { nullptr };
    static_assert(sizeof(term) == sizeof(termValue));
    memcpy(&term, &termValue, sizeof(termValue));
    auto ti = Execution::LoadTypeInfo(term.AsGlobal(), ectype, reinterpret_cast<void*>(frame.start));
    ectype->Put(IReg::IR1, Value::Primitive { .u64 = reinterpret_cast<uintptr_t>(ti.Raw()) });
    NEXT;
}

LABEL(LOAD_TI) {
    auto args = B9i64::Decode(reader);
    LOG_INSTR;
    auto ti = args.imm64.imm;
    ectype->Put(IReg::IR1, Value::Primitive { .u64 = ti });
    NEXT;
}

LABEL(IOF) {
    auto args = IOF::Decode(reader);
    LOG_INSTR;
    auto dst      = args.rr.x.IR();
    auto ref      = ectype->GetReference(args.rr.y.IR());
    auto typeInfo = TypeInfo(static_cast<uintptr_t>(args.imm64));
    ectype->Put(dst, Value::Primitive { .u64 = Execution::IsInstanceOf(ref, typeInfo) });
    NEXT;
}

LABEL(CATCH) {
    auto args = B2xr::Decode(reader);
    LOG_INSTR;
    auto exceptionObj = ectype->GetReference(IReg::IR_ACC);
    bool successful   = exceptionObj.value != 0;
    ectype->Put(args.xr.r.IR(), Value::Reference { .value = exceptionObj.value });
    NEXT_COND(successful);
}

LABEL(THROW) {
    auto args = B2xr::Decode(reader);
    LOG_INSTR;
    auto ref = ectype->GetReference(args.xr.r.IR());
    if (ref.value == 0) {
        FATAL("unexpected null in THROW");
    }
    THROW_EXPLICIT(ref.value);
}

#define ATOMIC_FETCH_CASE(OP, op, size)                                                                                \
    ATOMIC_FETCH_##OP##_##size: {                                                                                      \
        auto args = AtomicOp::Decode(reader);                                                                          \
        LOG_INSTR;                                                                                                     \
        interpreter.AtomicFetch<uint##size##_t>(                                                                       \
            args.rr1.x.IR(), args.rr1.y.IR(), args.rr2.x.IR(), args.offset,                                            \
            [](auto* atomicVal, auto val) { return atomicVal->fetch_##op(val, std::memory_order_seq_cst); }            \
        );                                                                                                             \
        NEXT;                                                                                                          \
    }

    ATOMIC_FETCH_CASE(ADD, add, 8)
    ATOMIC_FETCH_CASE(ADD, add, 16)
    ATOMIC_FETCH_CASE(ADD, add, 32)
    ATOMIC_FETCH_CASE(ADD, add, 64)
    ATOMIC_FETCH_CASE(SUB, sub, 8)
    ATOMIC_FETCH_CASE(SUB, sub, 16)
    ATOMIC_FETCH_CASE(SUB, sub, 32)
    ATOMIC_FETCH_CASE(SUB, sub, 64)
    ATOMIC_FETCH_CASE(AND, and, 8)
    ATOMIC_FETCH_CASE(AND, and, 16)
    ATOMIC_FETCH_CASE(AND, and, 32)
    ATOMIC_FETCH_CASE(AND, and, 64)
    ATOMIC_FETCH_CASE(OR,  or,  8)
    ATOMIC_FETCH_CASE(OR,  or,  16)
    ATOMIC_FETCH_CASE(OR,  or,  32)
    ATOMIC_FETCH_CASE(OR,  or,  64)
    ATOMIC_FETCH_CASE(XOR, xor, 8)
    ATOMIC_FETCH_CASE(XOR, xor, 16)
    ATOMIC_FETCH_CASE(XOR, xor, 32)
    ATOMIC_FETCH_CASE(XOR, xor, 64)
#undef ATOMIC_FETCH_CASE

#define CAS_CASE(size)                                                                                                 \
    CAS_##size: {                                                                                                      \
        auto args = AtomicOp::Decode(reader);                                                                          \
        LOG_INSTR;                                                                                                     \
        interpreter.CASPrim<uint##size##_t>(                                                                           \
            args.rr1.x.IR(), args.rr1.y.IR(), args.rr2.x.IR(), args.rr2.y.IR(), args.offset                            \
        );                                                                                                             \
        NEXT;                                                                                                          \
    }

    CAS_CASE(8)
    CAS_CASE(16)
    CAS_CASE(32)
    CAS_CASE(64)
#undef CAS_CASE

CAS_REF: {
    auto args = AtomicOp::Decode(reader);
    LOG_INSTR;
    interpreter.CASRef(args.rr1.x.IR(), args.rr1.y.IR(), args.rr2.x.IR(), args.rr2.y.IR(), args.offset);
    NEXT;
}

#define SWAP_CASE(size)                                                                                               \
    ATOMIC_SWAP_##size: {                                                                                             \
        auto args = AtomicOp::Decode(reader);                                                                         \
        LOG_INSTR;                                                                                                    \
        interpreter.AtomicSwapPrim<uint##size##_t>(                                                                   \
            args.rr1.x.IR(), args.rr1.y.IR(), args.rr2.x.IR(), args.offset                                            \
        );                                                                                                            \
        NEXT;                                                                                                         \
    }

    SWAP_CASE(8)
    SWAP_CASE(16)
    SWAP_CASE(32)
    SWAP_CASE(64)
#undef SWAP_CASE

ATOMIC_SWAP_REF: {
    auto args = AtomicOp::Decode(reader);
    LOG_INSTR;
    interpreter.AtomicSwapRef(args.rr1.x.IR(), args.rr1.y.IR(), args.rr2.x.IR(), args.offset);
    NEXT;
}

ATOMIC_LOAD: {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    interpreter.AtomicLoad(args.xi12.imm4.LDK(), args.rr.x.IR(), args.rr.y.IR(), args.xi12.imm12);
    NEXT;
}

ATOMIC_STORE: {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    interpreter.AtomicStore(args.xi12.imm4.STK(), args.rr.x.IR(), args.rr.y.IR(), args.xi12.imm12);
    NEXT;
}

LABEL(MEMSPACE) {
    auto args = B1::Decode(reader);
    LOG_INSTR;
    memspaceOffsetAcc = 0;
    MEM_NEXT;
}

    // -- MemSpace opcode table --

LABEL(MEM_HALT) {
    FATAL("halt");
    return {};
}

LABEL(OFFS16) {
    auto args = M3i16::Decode(reader);
    LOG_INSTR;
    memspaceOffsetAcc += interpreter.MemOffset(args.imm16);
    MEM_NEXT;
}
LABEL(OFFS32) {
    auto args = M5i32::Decode(reader);
    LOG_INSTR;
    memspaceOffsetAcc += interpreter.MemOffset(args.imm32);
    MEM_NEXT;
}
LABEL(OFFS64) {
    auto args = M9i64::Decode(reader);
    LOG_INSTR;
    memspaceOffsetAcc += interpreter.MemOffset(args.imm64);
    MEM_NEXT;
}
LABEL(OFFS_REG) {
    auto args = M2xr::Decode(reader);
    LOG_INSTR;
    memspaceOffsetAcc += interpreter.MemOffsetReg(args.xr.r.IR());
    MEM_NEXT;
}
LABEL(OFFS_REG_IDX64) {
    auto args = M10xri64::Decode(reader);
    LOG_INSTR;
    memspaceOffsetAcc += interpreter.MemOffsetReg(args.xr.r.IR()) * interpreter.MemOffset(args.imm64.imm);
    MEM_NEXT;
}
LABEL(R_READ_STRUCT) {
    auto args = MStructFieldOp::Decode(reader);
    LOG_INSTR;
    auto dst   = ectype->GetPrimitive(args.rr.x.IR()).u64;
    auto base  = ectype->GetReference(args.rr.y.IR());
    auto field = base.value + memspaceOffsetAcc;
    RTSupport::Execution::ReadStructField(dst, base, field, args.ti, handle);
    NEXT;
}
LABEL(R_WRITE_STRUCT) {
    auto args = MStructFieldOp::Decode(reader);
    LOG_INSTR;
    auto src   = ectype->GetPrimitive(args.rr.x.IR()).u64;
    auto base  = ectype->GetReference(args.rr.y.IR());
    auto field = base.value + memspaceOffsetAcc;
    RTSupport::Execution::WriteStructField(src, base, field, args.ti, handle);
    NEXT;
}

LABEL(DLD_GENERIC) {
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
LABEL(DST_GENERIC) {
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

LABEL(GENERIC_FIELD) {
    auto args = M6rri32::Decode(reader);
    LOG_INSTR;
    auto ti            = TypeInfo(ectype->GetPrimitive(args.rr.x.IR()).u64);
    auto offs          = RTSupport::Execution::GetFieldOffset(ti, args.imm32.imm, false);
    memspaceOffsetAcc += offs;
    MEM_NEXT;
}

#define RLD(ldk)                                                                                                       \
    LABEL(RLD_##ldk)                                                                                                   \
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
    RLD(LEA)
#undef RLD

#define RST(stk)                                                                                                       \
    LABEL(RST_##stk)                                                                                                   \
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
    LABEL(RSTI_##memSize##_##immSize)                                                                                  \
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
    LABEL(DLD_##ldk)                                                                                                   \
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
    DLD(LEA)
#undef DLD

#define DST(stk)                                                                                                       \
    LABEL(DST_##stk)                                                                                                   \
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
    LABEL(DSTI_##memSize##_##immSize)                                                                                  \
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
    LABEL(SLD_##ldk)                                                                                                   \
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
    SLD(LEA)
#undef SLD

#define SST(stk)                                                                                                       \
    LABEL(SST_##stk)                                                                                                   \
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
    LABEL(SSTI_##memSize##_##immSize)                                                                                  \
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
    LABEL(FLD_##ldk)                                                                                                   \
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
    FLD(LEA)
#undef FLD

#define FST(stk)                                                                                                       \
    LABEL(FST_##stk)                                                                                                   \
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
    LABEL(FSTI_##memSize##_##immSize)                                                                                  \
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

LABEL(COPY_REC_FROM_OBJ) {
    auto args = MStructFieldOp::Decode(reader);
    LOG_INSTR;
    auto to = ectype->GetReference(args.rr.y.IR());   // pointer to local record
    auto ti = args.ti;                                // typeinfo
    auto fromReg = args.rr.x.IR();
    if (fromReg == IReg::IRZ) {
        RTSupport::Execution::ReadStaticStruct(to.value, memspaceOffsetAcc, ti, handle);
        NEXT;
    } else {
        auto from = ectype->GetReference(fromReg);
        auto derived = from.value + memspaceOffsetAcc;
        // heap -> local (gc barrier required)
        RTSupport::Execution::ReadStructField(to.value, from, derived, ti, handle);
        NEXT;
    }
}

LABEL(COPY_REC_FROM_REC) {
    auto args = MStructFieldOp::Decode(reader);
    LOG_INSTR;
    auto from = ectype->GetReference(args.rr.x.IR()); // pointer to local record (src)
    auto to = ectype->GetReference(args.rr.y.IR());   // pointer to local record (dst)
    auto ti = TypeInfo(args.ti);                      // typeinfo
    auto recStart = from.value + memspaceOffsetAcc;   // interior record

    // local -> local (gc barrier isn't required)
    uint32_t size = MetaInfo::GetTypeSize(ti); // FIXME: encode size
    memcpy((void*) to.value, (void*) recStart, size);

    NEXT;
}

LABEL(COPY_REC_TO_OBJ) {
    auto args = MStructFieldOp::Decode(reader);
    LOG_INSTR;
    auto fromReg = args.rr.x.IR();
    auto from = ectype->GetReference(fromReg); // pointer to local record
    auto ti = args.ti;                         // typeinfo
    auto toReg = args.rr.y.IR();
    if (toReg == IReg::IRZ) {
        RTSupport::Execution::WriteStaticStruct(memspaceOffsetAcc, from.value, ti, handle);
        NEXT;
    } else {
        auto to = ectype->GetReference(toReg);       // derived base
        auto derived = to.value + memspaceOffsetAcc; // derived (interior record)
        // local -> heap (gc barrier required)
        RTSupport::Execution::WriteStructField(from.value, to, derived, ti, handle);
        NEXT;
    }
}

LABEL(COPY_REC_TO_REC) {
    auto args = MStructFieldOp::Decode(reader);
    LOG_INSTR;
    auto from = ectype->GetReference(args.rr.x.IR()); // pointer to local record
    auto to = ectype->GetReference(args.rr.y.IR());   // base
    auto ti = args.ti;                                // typeinfo
    auto recStart = to.value + memspaceOffsetAcc;     // interior record
    // local -> local (gc barrier isn't required)
    uint32_t size = MetaInfo::GetTypeSize(ti); // FIXME: encode size
    memcpy((void*) recStart, (void*) from.value, size);
    NEXT;
}

#undef MEM_NEXT
#undef NEXT
#undef NEXT_COND
}

// clang-format on

void engine_log_int_start(DynamicFunctionHandle* handle, Ectype* ectype)
{
    auto& logger = Log::interpretation.Stream(Logging::Level::DEBUG);
    auto id      = handle->methodDef.GetFileId().id;
    auto offs    = handle->methodDef.GetOffset().value;
    logger.PrintFmt("Started interpretation of %p (%u,%u) %ld", handle, id, offs, ectype->funcCtr);
    logger.NewLine();
}

void engine_log_int_end(DynamicFunctionHandle* handle, Ectype* ectype)
{
    auto& logger = Log::interpretation.Stream(Logging::Level::DEBUG);
    auto id      = handle->methodDef.GetFileId().id;
    auto offs    = handle->methodDef.GetOffset().value;
    logger.PrintFmt("Stopped interpretation of %p (%u,%u)", handle, id, offs);
    logger.NewLine();
    logger.Flush();
}
}

Thunk Interpretation::InterpretationLoop(
    Ectype* ectype, Frame frame, RTSupport::ThreadHandle handle, LiteralTable* literals, Decoder::ByteReader& reader0
)
{
    return engine_interpretation_loop(ectype, frame, handle, literals, reader0);
}

void Interpretation::InterpretationStart(DynamicFunctionHandle* handle, Ectype* ectype)
{
    engine_log_int_start(handle, ectype);
}

void Interpretation::InterpretationEnd(DynamicFunctionHandle* handle, Ectype* ectype)
{
    engine_log_int_end(handle, ectype);
}

RTSupport::TypeInfo Interpretation::builtinTypeInfos[BUILTIN_COUNT];
