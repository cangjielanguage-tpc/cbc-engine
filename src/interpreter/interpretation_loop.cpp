#include "interpretation_loop.h"
#include "cbc/formater_rt.h"
#include "cbc/frame.h"
#include "cbc/isa.h"
#include "cbc/isa_rewriter.h"
#include "cbc/isa_rt.h"
#include "engine/terms.h"
#include "interpreter.h"
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

[[gnu::always_inline]]
static inline uintptr_t ParamSlotPosition(uintptr_t frameBot, uint16_t arg)
{
    ASSERT(arg >= IReg::VIRT_COUNT);
    return frameBot + (arg - IReg::VIRT_COUNT) * Cbc::STACK_SLOT_SIZE;
}

[[gnu::always_inline]]
static inline TypeInfo ReadCallTypeInfo(Ectype* ectype, uintptr_t frameBot, uint16_t arg)
{
    ASSERT(arg != StaticCallTypeInfoArgs::NONE);
    if (arg < IReg::VIRT_COUNT) {
        return TypeInfo(ectype->GetPrimitive(IReg::From(arg)).u64);
    }
    return *reinterpret_cast<TypeInfo*>(ParamSlotPosition(frameBot, arg));
}

[[gnu::always_inline]]
static inline void WriteCallTypeInfo(Ectype* ectype, uintptr_t frameBot, uint16_t arg, TypeInfo ti)
{
    ASSERT(arg != StaticCallTypeInfoArgs::NONE);
    if (arg < IReg::VIRT_COUNT) {
        ectype->Put(IReg::From(arg), Value::Primitive { ti.UInt() });
    } else {
        *reinterpret_cast<TypeInfo*>(ParamSlotPosition(frameBot, arg)) = ti;
    }
}

[[gnu::always_inline]]
static inline TypeInfo InterfaceReceiverTypeInfo(Ectype* ectype, uintptr_t frameBot, bool sret, uint16_t thisTiArg)
{
    if (thisTiArg != StaticCallTypeInfoArgs::NONE) {
        return ReadCallTypeInfo(ectype, frameBot, thisTiArg);
    }
    auto receiver = HAS_SRET_SHIFT && sret ? IReg::IR2 : IReg::IR1;
    return Execution::GetTypeInfo(ectype->GetReference(receiver));
}

[[gnu::always_inline]]
static void StoreGeneric(StoreGenericOp args, Ectype* ectype, ThreadHandle handle)
{
    auto base    = ectype->GetReference(args.base);
    auto derived = ectype->GetReference(args.derived);
    auto obj     = ectype->GetReference(args.src);

    auto typeInfo = TypeInfo(ectype->GetPrimitive(args.ti).u64);
    if (RTSupport::Execution::IsReference(typeInfo)) {
        RTSupport::Execution::WriteObjectInstance(base, derived.value + args.offset, obj, handle);
    } else {
        uint32_t size = RTSupport::MetaInfo::GetTypeSize(typeInfo);
        RTSupport::Execution::WriteGeneric(base, derived.value + args.offset, obj, size, handle);
    }
}


template <bool opt1>
[[gnu::always_inline]]
static void InitSomeGeneric(Ectype*& ectype, RTSupport::ThreadHandle& handle, InitOption& args)
{
    auto rsrc = args.src;
    auto rti  = args.underlyingTi;

    auto ti   = TypeInfo(ectype->GetPrimitive(rti).u64);
    auto base = ectype->GetReference(IReg::IR_ACC);
    auto obj  = ectype->GetReference(rsrc);
    if (RTSupport::Execution::IsReference(ti)) {
        RTSupport::Execution::WriteObjectInstance(base, base.value + MetaInfo::ObjectHeaderSize(), obj, handle);
    } else {
        auto optTi    = RTSupport::Execution::GetTypeInfo(base);
        auto offs     = RTSupport::Execution::GetFieldOffset(optTi, 1, true);
        auto derived  = base.value + offs;
        uint32_t size = RTSupport::MetaInfo::GetTypeSize(ti);
        RTSupport::Execution::WriteGeneric(base, derived, obj, size, handle);
        if constexpr (opt1) {
            MemoryLocation(base.value, MetaInfo::ObjectHeaderSize()).StoreImm(StoreAccessKind::ST_8, 1);
        }
    }
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
/// which contains the `ThunkOp` index of the operation to run and a single argument.
///
/// We restrict the number of arguments in the `Thunk` structure to 1 to allow passing the
/// structure by value via registers in the System V x64 and AArch64 ABIs.
///
/// Note that the actual dispatch of `thunk.op` differs from the ASM in the unit test
/// framework.
Interpretation::Thunk engine_interpretation_loop(
    Ectype* ectype,
    Frame frame,
    RTSupport::ThreadHandle handle,
    LiteralTable* literals,
    Decoder::ByteReader& reader0,
    uintptr_t frameBot
)
{
#define NEXT goto* MAIN_TABLE[reader.PeekOpcode()]
#define NEXT_COND(successful) goto* MAIN_TABLE[(successful) ? reader.PeekOpcode() : 0]
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
        reader0                = reader; /* save current pc */                                                         \
        return { ThunkOp::HandleException, reinterpret_cast<void*>(exceptionObj) };                                    \
    } while (0)

#define THROW_IMPLICIT(type)                                                                                           \
    do {                                                                                                               \
        reader0 = reader;                                                                                              \
        return { ThunkOp::ThrowImplicit, reinterpret_cast<void*>(type) };                                              \
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
    static void* MAIN_TABLE[] = { CBC_RT_OPCODES(CBC_RT_LABEL) };

#ifdef NDEBUG
    // Fake use keeps decode-only locals (e.g. `args`) from tripping -Wunused-variable.
    #define LOG_INSTR (void)args;
    #define SET_LABEL(instr_name) (void)(instr_name)
    #define DEBUG_INFO(instr_name) LABEL_SYMBOL(instr_name)
#else
    // Logging format is
    // [int] (stack depth) < (bc pos): instruction
    #define LOG_INSTR                                                                                                  \
        if (ectype->funcCtr > Log::skipThreshold) {                                                                    \
            Log::interpretation.Log(Logging::TRACE, [&](Stream::Output& logger) {                                      \
                logger.PrintFmt("#0x%lx < 0x%03lx: ", ectype, pos - start);                                       \
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
    auto imsg = (InstMsg*)args.imm64.ptr;
    if (Cbc::emitLogInstructions) {
        Log::stream.PrintLn("{}: {}", Stream::Hex(imsg->rewrittenPos), (char*)imsg->msg);
    }
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
LABEL(STK_PARAM) {
    auto args = RT::StackParam::Decode(reader);
    LOG_INSTR;
    auto stk = Cbc::Format::StoreAccessKind::ST_64; // FIXME: encode for darwin_aarch64
    MemoryLocation(frameBot, args.dst).StorePrim(stk, args.src, ectype);
    NEXT;
}
LABEL(STK_PARAM_F) {
    auto args = RT::StackParamF::Decode(reader);
    LOG_INSTR;
    auto stk = Cbc::Format::StoreAccessKind::ST_F64;
    MemoryLocation(frameBot, args.dst).StorePrim(stk, args.src, ectype);
    NEXT;
}
LABEL(STK_PARAM_S16) {
    auto args = RT::StackParamS16::Decode(reader);
    LOG_INSTR;
    auto srcLoc = frame.start + args.src;
    auto dstLoc = frameBot + args.dst;
    *reinterpret_cast<uint64_t*>(dstLoc) = *reinterpret_cast<uint64_t*>(srcLoc);
    NEXT;
}
LABEL(STK_PARAM_S32) {
    auto args = RT::StackParamS32::Decode(reader);
    LOG_INSTR;
    auto srcLoc = frame.start + args.src;
    auto dstLoc = frameBot + args.dst;
    *reinterpret_cast<uint64_t*>(dstLoc) = *reinterpret_cast<uint64_t*>(srcLoc);
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

    return { ThunkOp::GcPoint, RTSupport::Execution::GcPoint() };
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
LABEL(SBIN8) {
    auto args = B3xrrr::Decode(reader);
    LOG_INSTR;
    bool successful =
        interpreter.template Binary<Width::W8>(args.xr.imm.Saturating(), args.xr.r.IR(), args.rr.x.IR(), args.rr.y.IR());
    NEXT_COND(successful);
}
LABEL(SBIN16) {
    auto args = B3xrrr::Decode(reader);
    LOG_INSTR;
    bool successful =
        interpreter.template Binary<Width::W16>(args.xr.imm.Saturating(), args.xr.r.IR(), args.rr.x.IR(), args.rr.y.IR());
    NEXT_COND(successful);
}
LABEL(SBIN32) {
    auto args = B3xrrr::Decode(reader);
    LOG_INSTR;
    bool successful =
        interpreter.template Binary<Width::W32>(args.xr.imm.Saturating(), args.xr.r.IR(), args.rr.x.IR(), args.rr.y.IR());
    NEXT_COND(successful);
}
LABEL(SBIN64) {
    auto args = B3xrrr::Decode(reader);
    LOG_INSTR;
    bool successful =
        interpreter.template Binary<Width::W64>(args.xr.imm.Saturating(), args.xr.r.IR(), args.rr.x.IR(), args.rr.y.IR());
    NEXT_COND(successful);
}
LABEL(SBINIMM8) {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    bool successful =
        interpreter.template BinaryImm<Width::W8>(
        args.xi12.imm4.Saturating(), args.rr.x.IR(), args.rr.y.IR(), MathUtils::SignExtend(static_cast<uint64_t>(args.xi12.imm12), 12)
    );
    NEXT_COND(successful);
}
LABEL(SBINIMM16) {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    bool successful =
        interpreter.template BinaryImm<Width::W16>(
        args.xi12.imm4.Saturating(), args.rr.x.IR(), args.rr.y.IR(), MathUtils::SignExtend(static_cast<uint64_t>(args.xi12.imm12), 12)
    );
    NEXT_COND(successful);
}
LABEL(SBINIMM32) {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    bool successful =
        interpreter.template BinaryImm<Width::W32>(
        args.xi12.imm4.Saturating(), args.rr.x.IR(), args.rr.y.IR(), MathUtils::SignExtend(static_cast<uint64_t>(args.xi12.imm12), 12)
    );
    NEXT_COND(successful);
}
LABEL(SBINIMM64) {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    bool successful =
        interpreter.template BinaryImm<Width::W64>(
        args.xi12.imm4.Saturating(), args.rr.x.IR(), args.rr.y.IR(), MathUtils::SignExtend(static_cast<uint64_t>(args.xi12.imm12), 12)
    );
    NEXT_COND(successful);
}
LABEL(SBINIMM8W) {
    auto args = BinarySaturating::Decode(reader);
    LOG_INSTR;
    bool successful =
        interpreter.template BinaryImm<Width::W8>(args.op, args.rr.x.IR(), args.rr.y.IR(), args.imm.imm);
    NEXT_COND(successful);
}
LABEL(SBINIMM16W) {
    auto args = BinarySaturating::Decode(reader);
    LOG_INSTR;
    bool successful =
        interpreter.template BinaryImm<Width::W16>(args.op, args.rr.x.IR(), args.rr.y.IR(), args.imm.imm);
    NEXT_COND(successful);
}
LABEL(SBINIMM32W) {
    auto args = BinarySaturating::Decode(reader);
    LOG_INSTR;
    bool successful =
        interpreter.template BinaryImm<Width::W32>(args.op, args.rr.x.IR(), args.rr.y.IR(), args.imm.imm);
    NEXT_COND(successful);
}
LABEL(SBINIMM64W) {
    auto args = BinarySaturating::Decode(reader);
    LOG_INSTR;
    bool successful =
        interpreter.template BinaryImm<Width::W64>(args.op, args.rr.x.IR(), args.rr.y.IR(), args.imm.imm);
    NEXT_COND(successful);
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
    reader0 = reader; // save current pc

    return { ThunkOp::NewObjectAcc, type.Raw() };
}
LABEL(NEWOBJ_G) {
    auto args = B2rr::Decode(reader);
    LOG_INSTR;
    auto tiReg = args.rr.x;
    auto type  = TypeInfo(ectype->GetPrimitive(tiReg.IR()).u64);

    // Puts result to `IR1`.
    reader0 = reader; // save current pc

    return { ThunkOp::NewObject, type.Raw() };
}
LABEL(NEWOBJ_PINNED_G) {
    auto args = B2rr::Decode(reader);
    LOG_INSTR;
    auto tiReg = args.rr.x;
    auto type  = TypeInfo(ectype->GetPrimitive(tiReg.IR()).u64);

    // Puts result to `IR1`.
    reader0 = reader; // save current pc

    return { ThunkOp::NewObjectPinned, type.Raw() };
}
LABEL(NEWOBJ) {
    auto args = B9i64::Decode(reader);
    LOG_INSTR;
    auto type = TypeInfo(static_cast<uintptr_t>(args.imm64.imm));

    // Puts result to `IR1`.
    reader0 = reader; // save current pc

    return { ThunkOp::NewObject, type.Raw() };
}
LABEL(NEWOBJ_PINNED) {
    auto args = B9i64::Decode(reader);
    LOG_INSTR;
    auto type = TypeInfo(static_cast<uintptr_t>(args.imm64.imm));

    // Puts result to `IR1`.
    reader0 = reader; // save current pc

    return { ThunkOp::NewObjectPinned, type.Raw() };
}
LABEL(NEWBOX) {
    auto args = B2xr::Decode(reader);
    LOG_INSTR;
    auto btype = builtinTypeInfos[args.xr.imm];

    // Puts result to `IR1`.
    reader0 = reader; // save current pc

    return { ThunkOp::NewObjectAcc, btype.Raw() };
}
LABEL(NEWBOX2) {
    auto args = B9i64::Decode(reader);
    LOG_INSTR;
    auto type = TypeInfo(static_cast<uintptr_t>(args.imm64.imm));

    // Puts result to `IR1`.
    reader0 = reader; // save current pc

    return { ThunkOp::NewObjectAcc, type.Raw() };
}
LABEL(UNBOX_T) {
    auto args = B6xri32::Decode(reader);
    LOG_INSTR;

    auto offs  = static_cast<int32_t>(args.imm32.imm);
    auto dst   = frame.start + offs;

    auto src   = ectype->GetReference(args.xr.r.IR());
    auto ti    = Execution::GetTypeInfo(src);
    auto field = src.value + RTSupport::MetaInfo::ObjectHeaderSize();

    RTSupport::Execution::ReadStructField(dst, src, field, ti, handle);
    NEXT;
}
LABEL(READ_STRUCT_FIELD) {
    auto args = StructFieldOp::Decode(reader);
    LOG_INSTR;
    auto dst  = ectype->GetPrimitive(args.rr.x.IR()).u64;
    auto base = ectype->GetReference(args.rr.y.IR());
    auto field = base.value + args.offset;
    RTSupport::Execution::ReadStructField(dst, base, field, args.ti, handle);
    NEXT;
}
LABEL(WRITE_STRUCT_FIELD) {
    auto args = StructFieldOp::Decode(reader);
    LOG_INSTR;
    auto src  = ectype->GetPrimitive(args.rr.x.IR()).u64;
    auto base = ectype->GetReference(args.rr.y.IR());
    auto field = base.value + args.offset;
    RTSupport::Execution::WriteStructField(src, base, field, args.ti, handle);
    NEXT;
}
LABEL(INITCLOSURE) {
    auto args = B1::Decode(reader);
    LOG_INSTR;
    Execution::InitializeClosure(ectype->GetReference(IReg::IR1), false);
    NEXT;
}
LABEL(INITCLOSURE_SRET) {
    auto args = B1::Decode(reader);
    LOG_INSTR;
    Execution::InitializeClosure(ectype->GetReference(IReg::IR1), true);
    NEXT;
}
LABEL(SPAWN_FUTURE) {
    auto args = B1::Decode(reader);
    LOG_INSTR;

    reader0 = reader; // save current pc

    return { ThunkOp::SpawnFuture, nullptr };
}
LABEL(SPAWN) {
    auto args = B9i64::Decode(reader);
    LOG_INSTR;
    auto type = TypeInfo(static_cast<uintptr_t>(args.imm64.imm));

    // TODO: it seems that spawn could be called directly
    reader0 = reader; // save current pc

    return { ThunkOp::Spawn, type.Raw() };
}
LABEL(NEWARR) {
    auto args = B9i64::Decode(reader);
    LOG_INSTR;
    auto type = TypeInfo(static_cast<uintptr_t>(args.imm64.imm));

    // Puts result to `IR1`, expects length to be passed on `IR2`.
    reader0 = reader; // save current pc

    return { ThunkOp::NewArray, type.Raw() };
}
LABEL(NEWARR_G) {
    auto args = B2rr::Decode(reader);
    LOG_INSTR;
    auto type = TypeInfo(ectype->GetPrimitive(args.rr.x.IR()).u64);

    // Puts result to `IR1`, expects length to be passed on `IR2`.
    reader0 = reader; // save current pc

    return { ThunkOp::NewArray, type.Raw() };
}
LABEL(LOAD_ADDR) {
    auto args = B4xri16::Decode(reader);
    LOG_INSTR;
    auto location   = literals->at(args.imm.imm).u64;
    bool successful = interpreter.LoadAddr(args.xr.imm.LDK(), args.xr.r, location);
    NEXT_COND(successful);
}
LABEL(STORE_ADDR) {
    auto args = B4xri16::Decode(reader);
    LOG_INSTR;
    auto location   = literals->at(args.imm.imm).u64;
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

LABEL(INDEX_GENERIC) {
    // This instruction is used only for CangjieArray and can't be used for VArray and Tuple.
    auto args = IndexGeneric::Decode(reader);
    LOG_INSTR;
    auto tiReg = args.idx.y;
    auto arrayTI = TypeInfo(ectype->GetPrimitive(tiReg.IR()).u64);
    auto ti = RTSupport::MetaInfo::GetElementTypeInfo(arrayTI);
    interpreter.LeaIndex(args.rr.x.IR(), args.rr.y.IR(), args.idx.x.IR(), ti);
    NEXT;
}

LABEL(CONST_INDEX_GENERIC) {
    // This instruction is used only for CangjieArray and can't be used for VArray and Tuple.
    auto args = ConstIndexGeneric::Decode(reader);
    LOG_INSTR;
    auto tiReg = args.ti.x;
    auto arrayTI = TypeInfo(ectype->GetPrimitive(tiReg.IR()).u64);
    auto ti = RTSupport::MetaInfo::GetElementTypeInfo(arrayTI);
    interpreter.LeaIndex(args.rr.x.IR(), args.rr.y.IR(), args.idx.imm, ti);
    NEXT;
}

LABEL(LOAD_FRAME_F)
LABEL(LOAD_FRAME) {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    auto offset = MathUtils::SignExtend(static_cast<int32_t>(args.xi12.imm12), 12);
    bool successful = interpreter.LoadFrame(args.xi12.imm4.LDK(), args.rr.x, offset);
    NEXT_COND(successful);
}
LABEL(STORE_FRAME_F)
LABEL(STORE_FRAME) {
    auto args = B4xi12rr::Decode(reader);
    LOG_INSTR;
    auto offset = MathUtils::SignExtend(static_cast<int32_t>(args.xi12.imm12), 12);
    bool successful = interpreter.StoreFrame(args.xi12.imm4.STK(), args.rr.x, offset);
    NEXT_COND(successful);
}
LABEL(LOAD_LONG_FRAME_F)
LABEL(LOAD_LONG_FRAME) {
    auto args = B7xrrri32::Decode(reader);
    LOG_INSTR;
    auto offset = static_cast<int32_t>(args.imm32.imm);
    bool successful = interpreter.LoadFrame(args.xr.imm.LDK(), args.xr.r, offset);
    NEXT_COND(successful);
}
LABEL(STORE_LONG_FRAME_F)
LABEL(STORE_LONG_FRAME) {
    auto args = B7xrrri32::Decode(reader);
    LOG_INSTR;
    auto offset = static_cast<int32_t>(args.imm32.imm);
    bool successful = interpreter.StoreFrame(args.xr.imm.STK(), args.xr.r, offset);
    NEXT_COND(successful);
}
LABEL(STORE_FRAME_IMM_1) {
    auto args = RT::StoreFrameImm::Decode1(reader);
    LOG_INSTR;
    bool successful = interpreter.StoreFrameImm(args.stk, args.imm, args.offs);
    NEXT_COND(successful);
}
LABEL(STORE_FRAME_IMM_2) {
    auto args = RT::StoreFrameImm::Decode2(reader);
    LOG_INSTR;
    bool successful = interpreter.StoreFrameImm(args.stk, args.imm, args.offs);
    NEXT_COND(successful);
}
LABEL(STORE_FRAME_IMM_4) {
    auto args = RT::StoreFrameImm::Decode4(reader);
    LOG_INSTR;
    bool successful = interpreter.StoreFrameImm(args.stk, args.imm, args.offs);
    NEXT_COND(successful);
}
LABEL(STORE_FRAME_IMM_8) {
    auto args = RT::StoreFrameImm::Decode8(reader);
    LOG_INSTR;
    bool successful = interpreter.StoreFrameImm(args.stk, args.imm, args.offs);
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
        return { .op = ThunkOp::LoadGeneric, .argUInt = packed };
    }
}
LABEL(STORE_GENERIC) {
    auto args = StoreGenericOp::Decode0(reader);
    LOG_INSTR;
    StoreGeneric(args, ectype, handle);
    NEXT;
}
LABEL(STORE_GENERIC_1) {
    auto args = StoreGenericOp::Decode1(reader);
    LOG_INSTR;
    StoreGeneric(args, ectype, handle);
    NEXT;
}
LABEL(STORE_GENERIC_4) {
    auto args = StoreGenericOp::Decode4(reader);
    LOG_INSTR;
    StoreGeneric(args, ectype, handle);
    NEXT;
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
LABEL(INIT_NONE_GENERIC_OPT0) {
    auto args = InitOption::Decode(reader);
    LOG_INSTR;
    auto rti = args.underlyingTi;
    auto ti  = TypeInfo(ectype->GetPrimitive(rti).u64);
    if (!RTSupport::Execution::IsReference(ti)) {
        bool successful = interpreter.StoreObjImm(Format::StoreAccessKind::ST_8, IReg::IR_ACC, MetaInfo::ObjectHeaderSize(), 1);
        NEXT_COND(successful);
    }
    NEXT;
}
LABEL(INIT_SOME_GENERIC_OPT0) {
    auto args = InitOption::Decode(reader);
    LOG_INSTR;
    InitSomeGeneric<false>(ectype, handle, args);
    NEXT;
}
LABEL(INIT_SOME_GENERIC_OPT1) {
    auto args = InitOption::Decode(reader);
    LOG_INSTR;
    InitSomeGeneric<true>(ectype, handle, args);
    NEXT;
}
LABEL(PREP_TYPED) {
    auto args = B13i64i32::Decode(reader);
    LOG_INSTR;
    auto typedOffset = static_cast<int32_t>(args.imm32.imm);
    auto size        = args.imm64.imm;
    memset(reinterpret_cast<uint8_t*>(frame.start) + typedOffset, 0, size);
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

    return { ThunkOp::I2ICall, reinterpret_cast<void*>(fuh) };
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

    return { ThunkOp::I2CCall, reinterpret_cast<void*>(target) };
}
LABEL(DIRECT_CALL_2N) {
    auto args = B4xi12i8::Decode(reader);
    LOG_INSTR;
    auto target  = literals->at(args.xi12.imm12).uintptr;
    auto packed = target
        | (static_cast<uint64_t>(args.xi12.imm4) << I2N_VEC_COUNT_SHIFT)
        | (static_cast<uint64_t>(args.imm8.imm) << I2N_STACK_ARGS_SIZE_SHIFT);

    reader0 = reader; // save current pc

    return { ThunkOp::I2NCall, reinterpret_cast<void*>(packed) };
}
LABEL(CALL_CFUNC) {
    auto args = B3xri8::Decode(reader);
    LOG_INSTR;
    auto target = ectype->GetPrimitive(IReg::From(args.xr.r)).u64;
    auto packed = target
        | (static_cast<uint64_t>(args.xr.imm) << I2N_VEC_COUNT_SHIFT)
        | (static_cast<uint64_t>(args.imm8.imm) << I2N_STACK_ARGS_SIZE_SHIFT);

    reader0 = reader; // save current pc

    return { ThunkOp::I2NCall, reinterpret_cast<void*>(packed) };
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

    auto typeInfo = InterfaceReceiverTypeInfo(ectype, frameBot, args.sret, args.thisTiArg);
    if (args.outerTiArg != StaticCallTypeInfoArgs::NONE) {
        WriteCallTypeInfo(ectype, frameBot, args.outerTiArg, Execution::GetMethodOuterTi(typeInfo, interf, num));
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
LABEL(INTERFACE_CALL_GENERIC) {
    auto args = InterfaceCallGeneric::Decode(reader);
    LOG_INSTR;
    auto num    = args.vnum;
    auto sret   = args.sret;
    auto interf = TypeInfo(ectype->GetPrimitive(IReg::IR_ACC).u64);

    auto typeInfo = InterfaceReceiverTypeInfo(ectype, frameBot, sret, args.thisTiArg);

    auto outerTI = Execution::GetMethodOuterTi(typeInfo, interf, num);

    WriteCallTypeInfo(ectype, frameBot, args.outerTiArg, outerTI);

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
    auto offs = (int32_t)args.imm32.imm;

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
    // FIXME: frame bot in unit tests?
    return engine_interpretation_loop(ectype, frame, handle, literals, reader0, 0);
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
