#include "isa_rewriter.h"
#include "cbc/abi.h"
#include "cbc/c_abi.h"
#include "cbc/emitter/emitter.h"
#include "cbc/emitter/symbols.h"
#include "cbc/formater_rt.h"
#include "cbc/frame.h"
#include "cbc/isa.h"
#include "cbc/isa_disasm.h"
#include "cbc/isa_parser.h"
#include "cbc/move_resolver.h"
#include "engine/engine.h"
#include "engine/image/flags.h"
#include "engine/resolving_output.h"
#include "engine/terms.h"
#include "interpreter/code.h"
#include "interpreter/function_handle.h"
#include "interpreter/interpretation_loop.h"
#include "interpreter/literals.h"
#include "interpreter/loggers.h"
#include "offsets_index.h"
#include "resolution/resolution.h"
#include "runtimesupport/runtime.h"
#include "utils/assertion.h"
#include "utils/logger.h"
#include "utils/math.h"
#include "utils/ostream.h"
#include "utils/reinterpretation.h"
#include "utils/span.h"
#include "utils/vector.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <sys/types.h>
#include <tuple>
#include <variant>

namespace Cbc {

using namespace Resolution;

using TK  = CbcTypeKind;
using LDK = Format::LoadAccessKind;
using STK = Format::StoreAccessKind;

enum class New {
    Obj,
    ObjPinned,
    Arr,
};

static LDK Ldk(CbcTypeKind tk)
{
    switch (tk) {
        case TK::U8:  return LDK::LD_U8;
        case TK::I8:  return LDK::LD_S8;
        case TK::U16: return LDK::LD_U16;
        case TK::I16: return LDK::LD_S16;
        case TK::U32:
        case TK::I32: return LDK::LD_32;
        case TK::U64:
        case TK::I64: return LDK::LD_64;
        case TK::F32: return LDK::LD_F32;
        case TK::F64: return LDK::LD_F64;

        case TK::BOOL: return LDK::LD_U8;
        case TK::REF:  return LDK::LD_REF;
        case TK::REC:  return LDK::LD_LEA; // record types: load effective address

        default: {
            FATAL("Not supported type kind %d", tk);
            return LDK::LD_S8;
        }
    }
}

static STK Stk(TK tk)
{
    switch (tk) {
        case TK::U8:
        case TK::I8:  return STK::ST_8;
        case TK::U16:
        case TK::I16: return STK::ST_16;
        case TK::U32:
        case TK::I32: return STK::ST_32;
        case TK::U64:
        case TK::I64: return STK::ST_64;
        case TK::F32: return STK::ST_F32;
        case TK::F64: return STK::ST_F64;

        case TK::BOOL: return STK::ST_8;
        case TK::REF:  return STK::ST_REF;

        default: {
            FATAL("Not supported type kind %d", tk);
            return STK::ST_8;
        }
    }
}

STK Stk(Interpretation::BuiltinType bt)
{
    switch (bt) {
        case Interpretation::BUILTIN_UNIT:    FATAL("Unit has no payload to store");
        case Interpretation::BUILTIN_BOOLEAN: return STK::ST_8;
        case Interpretation::BUILTIN_U8:      return STK::ST_8;
        case Interpretation::BUILTIN_I8:      return STK::ST_8;
        case Interpretation::BUILTIN_U16:     return STK::ST_16;
        case Interpretation::BUILTIN_I16:     return STK::ST_16;
        case Interpretation::BUILTIN_U32:     return STK::ST_32;
        case Interpretation::BUILTIN_I32:     return STK::ST_32;
        case Interpretation::BUILTIN_U64:     return STK::ST_64;
        case Interpretation::BUILTIN_I64:     return STK::ST_64;
        case Interpretation::BUILTIN_UADDR:   return STK::ST_64;
        case Interpretation::BUILTIN_IADDR:   return STK::ST_64;
        case Interpretation::BUILTIN_F16:     return STK::ST_16;
        case Interpretation::BUILTIN_F32:     return STK::ST_F32;
        case Interpretation::BUILTIN_F64:     return STK::ST_F64;
        case Interpretation::BUILTIN_RUNE:    return STK::ST_32;
        case Interpretation::BUILTIN_CSTRING: return STK::ST_64;
        case Interpretation::BUILTIN_NOTHING: return STK::ST_64; // anything allowed, ub
    }
}

LDK Ldk(Interpretation::BuiltinType bt)
{
    switch (bt) {
        case Interpretation::BUILTIN_UNIT:    FATAL("Unit has no payload to load");
        case Interpretation::BUILTIN_BOOLEAN: return LDK::LD_U8;
        case Interpretation::BUILTIN_U8:      return LDK::LD_U8;
        case Interpretation::BUILTIN_I8:      return LDK::LD_S8;
        case Interpretation::BUILTIN_U16:     return LDK::LD_U16;
        case Interpretation::BUILTIN_I16:     return LDK::LD_S16;
        case Interpretation::BUILTIN_U32:     return LDK::LD_32;
        case Interpretation::BUILTIN_I32:     return LDK::LD_32;
        case Interpretation::BUILTIN_U64:     return LDK::LD_64;
        case Interpretation::BUILTIN_I64:     return LDK::LD_64;
        case Interpretation::BUILTIN_UADDR:   return LDK::LD_64;
        case Interpretation::BUILTIN_IADDR:   return LDK::LD_64;
        case Interpretation::BUILTIN_F16:     return LDK::LD_U16;
        case Interpretation::BUILTIN_F32:     return LDK::LD_F32;
        case Interpretation::BUILTIN_F64:     return LDK::LD_F64;
        case Interpretation::BUILTIN_RUNE:    return LDK::LD_32;
        case Interpretation::BUILTIN_CSTRING: return LDK::LD_64;
        case Interpretation::BUILTIN_NOTHING: return LDK::LD_64; // anything allowed, ub
    }
}

struct RewriterMoveEmitter : public MoveEmitter {
    FrameLayout& frameLayout;
    Emitter::Emitter& emit;

    RewriterMoveEmitter(FrameLayout& frameLayout, Emitter::Emitter& emit) : frameLayout(frameLayout), emit(emit) {}

    void AssignIReg(IReg dst, IReg src) override { emit.Mov(dst, src); }

    void AssignFReg(FReg dst, FReg src) override { emit.Mov(dst, src); }

    void AssignParamSlot(int paramSlot, int stackSlot) override
    {
        auto srcOffset = frameLayout.UntypedSlotOffset(stackSlot);
        auto dstOffset = static_cast<uint16_t>(paramSlot) * 8;
        if (MathUtils::IsNBitsSigned(srcOffset, 16)) {
            emit.StackParamS16(srcOffset, dstOffset);
        } else {
            emit.StackParamS32(srcOffset, dstOffset);
        }
    }

    void AssignFRegFromSlot(FReg dst, int slot) override
    {
        // FIXME: load/stores must be typed
        emit.LoadFrame(LDK::LD_F64, dst, frameLayout.UntypedSlotOffset(slot));
    }

    void AssignIRegFromSlot(IReg dst, int slot) override
    {
        // FIXME: load/stores must be typed
        emit.LoadFrame(LDK::LD_64, dst, frameLayout.UntypedSlotOffset(slot));
    }

    void AssignSlotFromIReg(int slot, IReg src) override { emit.StackParam(src, static_cast<uint16_t>(slot) * 8); }

    void AssignSlotFromFReg(int slot, FReg src) override { emit.StackParamF(src, static_cast<uint16_t>(slot) * 8); }

    void AssignIRegFromAddr(IReg dst, Location addr, int32_t offset, Format::LoadAccessKind ldk) override
    {
        emit.LoadRec(ldk, dst, AddrReg(addr), offset);
    }

    void AssignFRegFromAddr(FReg dst, Location addr, int32_t offset, Format::LoadAccessKind ldk) override
    {
        emit.LoadRec(ldk, dst, AddrReg(addr), offset);
    }

    void AssignSlotFromAddr(int paramSlot, Location addr, int32_t offset, Format::LoadAccessKind ldk) override
    {
        auto base = AddrReg(addr);
        emit.LoadRec(ldk, SCRATCH_FOR_PBV, base, offset);
        emit.StackParam(SCRATCH_FOR_PBV, static_cast<uint16_t>(paramSlot) * 8);
    }

    IReg AddrReg(Location addr)
    {
        if (addr.Kind() == Location::IREG) {
            return IReg::From(addr.IRegIdx());
        }
        emit.LoadFrame(
            LDK::LD_64, SCRATCH_FOR_PBV, frameLayout.UntypedSlotOffset(static_cast<uint16_t>(addr.SlotIdx()))
        );
        return SCRATCH_FOR_PBV;
    }

    // This register is used for different operations during interpreation.
    // It's expected that on the moment of foreign call this register doesn't
    // contain any value that is needed after call.
    static constexpr IReg SCRATCH_FOR_PBV = IReg::IR_ACC;
};

struct IsaRewriter : public IsaParser {
    IsaRewriter(
        Resolver& resolver,
        Engine::Session& session,
        Image::Identifier<Image::MethodDefinition> method,
        MethodCode& code,
        FrameLayout& frameLayout,
        Emitter::Emitter& emit
    )
        : IsaParser(code),
          session(session),
          method(method),
          fileId(method.GetFileId()),
          resolver(resolver),
          code(code),
          emit(emit),
          frameLayout(frameLayout),
          bytecodeSize(reader.End() - reader.Start()),
          startPosition(0)
    {}

    Engine::Session& session;
    Image::Identifier<Image::MethodDefinition> method;
    Image::FileId fileId;
    Resolver& resolver;
    MethodCode& code;
    Emitter::Emitter& emit;
    FrameLayout& frameLayout;
    size_t bytecodeSize;
    uint32_t maxParamPassingSize = 0;
    Utils::Vector<uint32_t> pendingParamRefSlots;
    Utils::Vector<uint32_t> pendingParamRecSlots;
    std::optional<int> pendingDerived;
    Utils::Vector<CAbiPiece> sretRegPieces;
    Stream::Output& errStream = Interpretation::Log::preparation.Stream(Logging::Level::ERROR);

    size_t startPosition;
    std::unordered_map<ssize_t, Emitter::Label> instructionLabel;

    // positions, where GC metadata is expected to be attached
    struct StatePoint {
        Emitter::Label label; // position in rewritten code
        ssize_t originalPos;  // position in original code
        Utils::Vector<uint32_t> paramRefSlots;
        Utils::Vector<uint32_t> paramRecSlots;
        std::optional<int> derivedLoc;
    };

    Utils::Vector<StatePoint> statePoints;

    struct FailureMessage {
        size_t position;
        std::string message;
    };

    Utils::Vector<FailureMessage> failureMessages;

    InstructionOffsetsIndex BuildOffsetsIndex() { return InstructionOffsetsIndex::Create(emit, instructionLabel); }

    Emitter::Label InstructionLabel(ssize_t position)
    {
        ASSERTION(position >= 0, "label position is negative");
        ASSERTION(position <= bytecodeSize, "label position greater than bytecode size");
        if (auto existing = instructionLabel.find(position); existing != instructionLabel.end()) {
            return existing->second;
        } else {
            auto label = emit.NewLabel();
            instructionLabel.insert({ position, label });
            return label;
        }
    }

    void BindStatePoint()
    {
        auto label = emit.NewLabel();
        emit.Bind(label);
        StatePoint point {
            .label       = label,
            .originalPos = Pos(), // attached to the end of instruction
        };
        point.paramRefSlots = std::move(pendingParamRefSlots);
        point.paramRecSlots = std::move(pendingParamRecSlots);
        point.derivedLoc    = std::move(pendingDerived);
        pendingDerived      = std::nullopt;
        pendingParamRefSlots.Clear();
        pendingParamRecSlots.Clear();
        statePoints.PushBack(point);
    }

    template <typename Method> void EmitLogCall(std::string_view prefix, Method m)
    {
        if (Interpretation::Log::interpretation.GetLogLevel() < Logging::Level::TRACE) {
            return;
        }
        Stream::StringBuffer stream;
        stream << startPosition << ": " << prefix << ' ' << m;
        emit.LogInstruction(stream.ToCString());
    }

    char* returnedToMsg = nullptr;

    void EmitReturnedTo()
    {
        if (Interpretation::Log::interpretation.GetLogLevel() < Logging::Level::TRACE) {
            return;
        }
        if (!returnedToMsg) {
            Stream::StringBuffer buf;
            Stream::ResolvingOutput out(session, buf);
            out << "Returned to: " << method << ' ' << Stream::Detailed(method);
            returnedToMsg = buf.ToCString();
        }
        emit.LogInstruction(returnedToMsg);
    }

    void EmitFillSRetAreaByRegs()
    {
        if (sretRegPieces.Size() == 0) {
            return;
        }
        static constexpr IReg retIRegs[] = { IReg::IR1, IReg::IR2 };
        static constexpr FReg retFRegs[] = { FReg::FR0, FReg::FR1, FReg::FR2, FReg::FR3 };
        int intIdx                       = 0;
        int floatIdx                     = 0;
        for (const auto& piece : sretRegPieces) {
            if (IsFloat(piece.kind)) {
                emit.StoreRec(Stk(piece.kind), retFRegs[floatIdx++], IReg::IR_ACC, piece.offset);
            } else {
                emit.StoreRec(Stk(piece.kind), retIRegs[intIdx++], IReg::IR_ACC, piece.offset);
            }
        }
        sretRegPieces.Clear();
    }

    ssize_t Pos()
    {
        auto start  = reader.Start();
        auto cursor = reader.Cursor();
        return cursor - start;
    }

    void AdjustReg(IReg expected, IReg actual)
    {
        if (expected != actual) {
            emit.Mov(expected, actual);
        }
    }

    struct CallAbiFlags {
        bool isSRet;
        bool hasOuterTi;
        Image::MethodAbiKind abiKind;
    };

    // By-reference struct argument: the content copy and the pointer to
    // it are staged after the regular argument moves, because the copy area sits
    // after all stack arguments.
    struct ForeignStructCopy {
        Location srcAddr; // location of caller-owned struct
        Location ptrLoc;  // location where pointer to callee-owned struct copy should be saved
        uint32_t size;
        uint32_t dstOffset = 0; // byte offset of the callee-owned copy from the frame bottom
    };

    bool CheckArgCount(size_t argCount, CallAbiFlags flags, size_t funcVarCount, size_t sigParamCount)
    {
        size_t expected  = 0;
        expected        += flags.isSRet;
        switch (flags.abiKind) {
            case Image::MethodAbiKind::STATIC:         expected += 0; break;
            case Image::MethodAbiKind::HAS_THIS_TI:
            case Image::MethodAbiKind::REF_RECEIVER:
            case Image::MethodAbiKind::REC_RECEIVER:
            case Image::MethodAbiKind::PRIM_RECEIVER:
            case Image::MethodAbiKind::FPRIM_RECEIVER: expected += 1; break;
            case Image::MethodAbiKind::MUT:            expected += 2; break;
        }
        expected += sigParamCount;
        expected += funcVarCount;
        expected += flags.hasOuterTi;

        if (expected != argCount) {
            Stream::StringBuffer buf;
            buf.PrintLn(
                "Expected = {}. Actual = {}. ps = {}, ftvs = {}, sret = {}, outerti = {}, abi_kind = {}",
                expected,
                argCount,
                sigParamCount,
                funcVarCount,
                flags.isSRet,
                flags.hasOuterTi,
                flags.abiKind
            );
            Fail(buf.ToString());
            return false;
        }
        return true;
    }

    /// Stages arguments of a managed (Cangjie ABI) call.
    /// @return stack arguments count and fregs count
    std::tuple<int, int> StageManagedCallArgs(
        Utils::Span<const Location> args,
        CallAbiFlags flags,
        Resolution::MethodSignature& sig,
        Location* outerTiLoc = nullptr,
        Location* thisTiLoc  = nullptr,
        bool genericParams   = false
    )
    {
        ASSERT(!sig.isVarArg);
        int funcVarsCount = sig.tvars.GetLength();
        if (!CheckArgCount(args.Size(), flags, funcVarsCount, sig.ParamCount())) {
            return {};
        }

        pendingParamRefSlots.Clear();
        pendingParamRecSlots.Clear();
        MoveResolver moves;
        TrackedAbiAssigner abi([&](Location target, CbcTypeKind kind) {
            if (target.Kind() != Location::SLOT) {
                if (kind == CbcTypeKind::REF)
                    pendingParamRefSlots.PushBack(target.SlotIdx());
                if (kind == CbcTypeKind::REC)
                    pendingParamRecSlots.PushBack(target.SlotIdx());
            }
        });

        int idx = 0;
        if (flags.isSRet) {
            moves.AddMove(abi.ConsumeSret(), args[idx++]);
        }
        switch (flags.abiKind) {
            case Image::MethodAbiKind::MUT: {
                auto derived    = args[idx++];
                auto base       = args[idx++];
                auto derivedLoc = abi.Consume(CbcTypeKind::REC);
                auto baseLoc    = abi.Consume(CbcTypeKind::REF);
                moves.AddMove(derivedLoc, derived);
                moves.AddMove(baseLoc, base);
                ASSERT(derivedLoc.Kind() == Location::IREG);
                pendingDerived = derivedLoc.IRegIdx();
                break;
            }
            case Image::MethodAbiKind::REF_RECEIVER: {
                moves.AddMove(abi.Consume(CbcTypeKind::REF), args[idx++]);
                break;
            }
            case Image::MethodAbiKind::PRIM_RECEIVER: {
                moves.AddMove(abi.Consume(CbcTypeKind::I64), args[idx++]);
                break;
            }
            case Image::MethodAbiKind::FPRIM_RECEIVER: {
                moves.AddMove(abi.Consume(CbcTypeKind::F64), args[idx++]);
                break;
            }
            case Image::MethodAbiKind::REC_RECEIVER: {
                moves.AddMove(abi.Consume(CbcTypeKind::REC), args[idx++]);
                break;
            }
            case Image::MethodAbiKind::STATIC:
            case Image::MethodAbiKind::HAS_THIS_TI:;
        }

        auto params = sig.Params();
        ASSERT(args.Size() >= params.Size() + idx);

        if (genericParams) {
            // Special case for call.closure.g
            // The referenced type (of closure) in instruction doesn't encode parameters as boxes.
            for (int j = 0; j < params.Size(); ++idx, ++j) {
                moves.AddMove(abi.Consume(CbcTypeKind::REF), args[idx]);
            }
        } else {
            for (int j = 0; j < params.Size(); ++j) {
                moves.AddMove(abi.Consume(params[j]), args[idx++]);
            }
        }

        ASSERT(funcVarsCount <= args.Size());
        for (int j = 0; j < funcVarsCount; idx++, j++) {
            moves.AddMove(abi.Consume(CbcTypeKind::I64), args[idx]);
        }

        if (flags.hasOuterTi) {
            Location loc = abi.Consume(CbcTypeKind::I64);
            moves.AddMove(loc, args[idx++]);
            if (outerTiLoc)
                *outerTiLoc = loc;
        }

        if (flags.abiKind == Image::MethodAbiKind::HAS_THIS_TI) {
            Location loc = abi.Consume(CbcTypeKind::I64);
            moves.AddMove(loc, args[idx++]);
            if (thisTiLoc)
                *thisTiLoc = loc;
        }

        RewriterMoveEmitter emitter(frameLayout, emit);
        if (!moves.Resolve(emitter)) {
            Fail("Register kind mismatch");
            return {};
        }

        maxParamPassingSize =
            std::max(maxParamPassingSize, static_cast<uint32_t>(abi.MaxStackSlot()) * Cbc::STACK_SLOT_SIZE);
        return { abi.MaxStackSlot(), abi.MaxFRegs() };
    }

    /// Stages arguments of a foreign (C ABI) call.
    ///
    /// Note: IR_ACC is used heavily during foreign calls:
    ///     1) Can be used as scratch register during arguments resolution;
    ///     2) Can be used as destination caller-allocated buffer for struct return value.
    ///
    /// @return stack arguments count and fregs count
    std::tuple<int, int> StageForeignCallArgs(
        Utils::Span<const Location> args, CallAbiFlags flags, Resolution::MethodSignature& sig
    )
    {
        ASSERT(sig.tvars.GetLength() == 0);
        ASSERT(flags.abiKind == Image::MethodAbiKind::STATIC);
        ASSERT(!flags.hasOuterTi);

        int varArgLabelsCnt = (sig.isVarArg ? 1 : 0);
        if (!CheckArgCount(args.Size(), flags, 0, sig.ParamCount() - varArgLabelsCnt)) {
            return {};
        }

        MoveResolver moves;
        TrackedAbiAssigner abi([&](Location target, CbcTypeKind kind) {
            // Foreign functions cannot receive managed references.
            ASSERT(kind != CbcTypeKind::REF);
        });
        Utils::Vector<ForeignStructCopy> structCopies;

        sretRegPieces.Clear();
        if (flags.isSRet) {
            // Foreign record returned: can be placed on registers or in caller frame depending on classification.
            if (!StageForeignStructRet(args[args.Size() - 1], sig.ResType().term, moves, abi)) {
                Fail("failed to stage foreign record return");
                return {};
            }
        }

        int idx     = 0;
        auto params = sig.Params();
        ASSERT(args.Size() >= params.Size() + idx - varArgLabelsCnt);

        bool isVarArg = false;
        for (int j = 0; j < params.Size(); ++j) {
            if (params[j].GetKind() == Engine::TermKind::VARARGLABEL) {
                isVarArg = true;
                continue;
            }

            if (params[j].GetKind() == Engine::TermKind::VARRAY) {
                // VArrays are always passed by reference.
                moves.AddMove(abi.Consume(CbcTypeKind::I64), args[idx++]);
            } else if (params[j].IsRecord()) {
                // Pass struct argument by value.
                if (!StageForeignStructArg(args[idx++], params[j], isVarArg, moves, abi, structCopies)) {
                    Fail("failed to stage by-value struct argument");
                    return {};
                }
            } else if (!isVarArg) {
                moves.AddMove(abi.Consume(params[j]), args[idx++]);
            } else {
                moves.AddMove(abi.ConsumeVarArg(params[j]), args[idx++]);
            }
        }

        // By-reference struct copies are placed after all stack arguments:
        // the I2N adapter copies only the stack-argument area, so each copy
        // stays in the frame and only its pointer is passed.
        uint32_t copyOffset = static_cast<uint32_t>(abi.MaxStackSlot()) * Cbc::STACK_SLOT_SIZE;
        for (auto& copy : structCopies) {
            copy.dstOffset = copyOffset;
            for (uint32_t offset = 0; offset < copy.size; offset += Cbc::STACK_SLOT_SIZE) {
                moves.AddLoadFromAddr(
                    Location::Slot((copyOffset + offset) / Cbc::STACK_SLOT_SIZE), copy.srcAddr, offset, LDK::LD_64
                );
            }
            copyOffset += MathUtils::AlignUp(copy.size, Cbc::STACK_SLOT_SIZE);
        }

        RewriterMoveEmitter emitter(frameLayout, emit);
        if (!moves.Resolve(emitter)) {
            Fail("Register kind mismatch");
            return {};
        }

        for (const auto& copy : structCopies) {
            // Done after moves resolve, so copy.ptrLoc doesn't contain any useful data.
            if (copy.ptrLoc.Kind() == Location::SLOT) {
                emit.LeaStackParam(RewriterMoveEmitter::SCRATCH_FOR_PBV, copy.dstOffset);
                emit.StackParam(RewriterMoveEmitter::SCRATCH_FOR_PBV, static_cast<uint16_t>(copy.ptrLoc.SlotIdx()) * 8);
            } else {
                emit.LeaStackParam(IReg::From(copy.ptrLoc.IRegIdx()), copy.dstOffset);
            }
        }

        if (sretRegPieces.Size() > 0) {
            // Move sret destination address into ACC. It will be used
            // after call by `EmitFillSRetAreaByRegs`
            auto buf = args[args.Size() - 1];
            if (buf.Kind() == Location::SLOT) {
                emitter.AssignIRegFromSlot(IReg::IR_ACC, buf.SlotIdx());
            } else {
                emitter.AssignIReg(IReg::IR_ACC, IReg::From(buf.IRegIdx()));
            }
        }

        maxParamPassingSize = std::max(maxParamPassingSize, copyOffset);
        return { abi.MaxStackSlot(), abi.MaxFRegs() };
    }

    // Stages the result buffer of a foreign record return: the buffer is the last call
    // argument and is either the hidden sret pointer or the destination of the
    // register-returned value, captured into after the call by EmitFillSRetAreaByRegs.
    bool StageForeignStructRet(Location buf, Engine::Term type, MoveResolver& moves, AbiAssigner& abi)
    {
        Utils::Vector<CAbiField> fields0;
        if (!FlattenCAbiFields(resolver, type, 0, fields0)) {
            return false;
        }

        UNWRAP_OPT_B(size, resolver.fieldManager->GetFlatSize(type));
        auto fields = Utils::Span<CAbiField>(fields0.Data(), fields0.Size());
        auto plan   = ClassifyCAbiStruct(fields, size, false, 2, 4);
        switch (plan.passBy) {
            case CAbiPassBy::EMPTY: break;
            case CAbiPassBy::REGS:  {
                int needIRegs = 0;
                int needFRegs = 0;
                for (const auto& piece : plan.pieces) {
                    IsFloat(piece.kind) ? needFRegs++ : needIRegs++;
                }
                if (needIRegs > 2 || needFRegs > 4) {
                    return false;
                }
                for (const auto& piece : plan.pieces) {
                    sretRegPieces.PushBack(piece);
                }
                break;
            }
            case CAbiPassBy::STACK:
            case CAbiPassBy::REF:   moves.AddMove(abi.ConsumeSret(), buf); break;
        }
        return true;
    }

    bool StageForeignStructArg(
        Location srcAddr,
        Engine::Term type,
        bool varArgPos,
        MoveResolver& moves,
        AbiAssigner& abi,
        Utils::Vector<ForeignStructCopy>& copies
    )
    {
        Utils::Vector<CAbiField> fields0;
        if (!FlattenCAbiFields(resolver, type, 0, fields0)) {
            return false;
        }

        UNWRAP_OPT_B(size, resolver.fieldManager->GetFlatSize(type));
        auto fields = Utils::Span<CAbiField>(fields0.Data(), fields0.Size());
        auto plan   = ClassifyCAbiStruct(fields, size, varArgPos, abi.FreeIRegs(), abi.FreeFRegs());
        switch (plan.passBy) {
            case CAbiPassBy::EMPTY: break;
            case CAbiPassBy::REGS:  {
                for (const auto& piece : plan.pieces) {
                    auto loc = abi.Consume(piece.kind);
                    moves.AddLoadFromAddr(loc, srcAddr, piece.offset, Ldk(piece.kind));
                }
                break;
            }
            case CAbiPassBy::STACK: {
                int base = abi.ConsumeStackArea(size);
                // Place struct in stack args area.
                for (uint32_t offset = 0; offset < size; offset += Cbc::STACK_SLOT_SIZE) {
                    moves.AddLoadFromAddr(
                        Location::Slot(base + offset / Cbc::STACK_SLOT_SIZE), srcAddr, offset, LDK::LD_64
                    );
                }
                break;
            }
            case CAbiPassBy::REF: {
                // Stage placing struct after all args and consume location for reference.
                copies.PushBack(ForeignStructCopy { srcAddr, abi.Consume(CbcTypeKind::I64), size });
                break;
            }
        }
        return true;
    }

    void Bcc(Format::Width width, Format::CC cc, AnyReg l, AnyReg r, int64_t delta) override
    {
        if (cc.IsFloatingPoint()) {
            // FIXME: support floats
            emit.Bcc(cc, width, FReg::From(l), FReg::From(r), InstructionLabel(Pos() + delta));
        } else {
            emit.Bcc(cc, width, IReg::From(l), IReg::From(r), InstructionLabel(Pos() + delta));
        }
    }

    void BccImm(Format::Width width, Format::CC cc, IReg l, uint64_t imm, int64_t delta) override
    {
        emit.BccImm(cc, width, IReg::From(l), imm, InstructionLabel(Pos() + delta));
    }

    void Nop() override { emit.Nop(); }

    void Jump(int64_t delta) override { emit.Jmp(InstructionLabel(Pos() + delta)); }

    void Mov(Format::Width width, IReg d, IReg s) override { emit.Mov(d, s); }

    void FMov(Format::Width width, FReg d, FReg s) override { emit.Mov(d, s); }

    void FloatToInt(Format::Width width, IReg d, FReg s) override { emit.Mov(d, s); }

    void IntToFloat(Format::Width width, FReg d, IReg s) override { emit.Mov(d, s); }

    void MovRef(IReg d, IReg s) override { emit.Mov(d, s); }

    void MovImm(Format::Width width, IReg d, uint64_t value) override { emit.MovImm(width, d, value); }

    virtual void FMovImm(Format::Width width, FReg d, double value) override
    {
        if (width == Format::Width::W32) {
            emit.FMovI32(d, (float)value);
        } else {
            emit.FMovI64(d, value);
        }
    }

    void Binary(Format::Common op, Format::Width width, IReg d, IReg l, IReg r) override
    {
        emit.Binary(op, width, d, l, r);
    }

    void BinaryImm(Format::Common op, Format::Width width, IReg d, IReg l, uint64_t value) override
    {
        emit.BinaryImm(op, width, d, l, value);
    }

    void CBinary(Format::Checked op, Format::Width width, IReg d, IReg l, IReg r) override
    {
        emit.Binary(op, width, d, l, r);
        BindStatePoint();
    }

    void CBinaryImm(Format::Checked op, Format::Width width, IReg d, IReg l, uint64_t value) override
    {
        emit.BinaryImm(op, width, d, l, value);
        BindStatePoint();
    }

    void SBinary(Format::Saturating op, Format::Width width, IReg d, IReg l, IReg r) override
    {
        emit.SatBinary(op, width, d, l, r);
    }

    void SBinaryImm(Format::Saturating op, Format::Width width, IReg d, IReg l, uint64_t value) override
    {
        emit.SatBinaryImm(op, width, d, l, value);
    }

    void FBinary(Format::FloatOperations op, Format::Width width, FReg d, FReg l, FReg r) override
    {
        emit.Binary(op, width, d, l, r);
    }

    void FUnary(Format::FloatOperations op, Format::Width width, FReg d, FReg s) override
    {
        emit.Unary(op, width, d, s);
    }

    void Convert(Format::ConvertType toType, Format::ConvertType fromType, AnyReg to, AnyReg from) override
    {
        emit.Convert(toType, fromType, to, from);
    }

    void CheckedConvert(Format::ConvertType toType, Format::ConvertType fromType, AnyReg to, AnyReg from) override
    {
        emit.CheckedConvert(toType, fromType, to, from);
    }

    void MovBasePtr(IReg dst, bool local) override
    {
        auto basePtr = local ? RTSupport::Execution::GetLocalBasePtr() : RTSupport::Execution::GetGlobalBasePtr();
        emit.MovImm(Format::Width::W64, dst, basePtr.value);
    }

    void BFX(IReg dst, IReg src, Format::Width resW, Format::Width argW, bool sx, uint8_t offset, uint8_t size) override
    {
        // We can ignore resW and argW, because interpreter computes the result in 64-bit number anyway.
        if (sx) {
            emit.BFXS(dst, src, offset, size);
        } else {
            emit.BFXZ(dst, src, offset, size);
        }
    }

    void PrepareRecord(uint16_t ts) override {}

    void NewArr(IReg dst, IReg len, uint32_t typeId) override
    {
        AdjustReg(IReg::IR2, len);
        NewObject(dst, typeId, New::Arr);
    }

    void NewArrGeneric(IReg dst, IReg len, IReg ti) override
    {
        AdjustReg(IReg::IR2, len);
        emit.NewArrGeneric(ti);
        BindStatePoint();
        AdjustReg(dst, IReg::IR1);
    }

    void GcPoint() override
    {
        emit.GcPoint();
        BindStatePoint();
    }

    void ZeroVal(AnyReg dst, IReg ti) override
    {
        auto refPath = emit.NewLabel();
        auto end     = emit.NewLabel();
        emit.BranchIfRef(ti, refPath);
        {
            emit.NewObjGenericOnAcc(ti);
            BindStatePoint();
            AdjustReg(IReg::From(dst), IReg::IR_ACC);
        }
        emit.Jmp(end);
        emit.Bind(refPath);
        emit.MovImm(Format::Width::W64, IReg::From(dst), 0);
        emit.Bind(end);
    }

    virtual void FMathUnary32(Format::FloatMathOp op, FReg dst, FReg src) override
    {
        emit.FMathUnary(op, Format::Width::W32, dst, src);
    }

    virtual void FMathUnary64(Format::FloatMathOp op, FReg dst, FReg src) override
    {
        emit.FMathUnary(op, Format::Width::W64, dst, src);
    }

    void Ld(AnyReg dst, IReg base, uint32_t fieldId) override
    {
        UNWRAP_OPT(field, resolver.Query(Index<InstanceField>(fieldId)), Fail);
        UNWRAP_OPT(offset, field->offset, [&]() {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
        });
        auto kind = Ldk(field->fieldType.GetKind());
        if (field->refType.term.IsRecord()) {
            emit.LoadRec(kind, dst, base, offset);
        } else {
            emit.LoadObj(kind, dst, base, offset);
        }
    }

    void LdStatic(AnyReg dst, uint32_t fieldId) override
    {
        UNWRAP_OPT(field, resolver.Query(Index<StaticField>(fieldId)), Fail);
        UNWRAP_OPT(location, field->location, Fail);
        auto symbol = emit.NewAddressSym(location);
        emit.LoadStatic(Ldk(field->fieldType.GetKind()), dst, symbol);
    }

    int32_t TypedSlotOffset(uint32_t typedSlot)
    {
        return (int32_t)frameLayout.typedOffset[typedSlot] - frameLayout.frameSize;
    }

    std::optional<RTSupport::TypeInfo> TypeSlotTypeInfo(uint16_t slot)
    {
        auto type = resolver.Query(Index<Type>(code.StackAllocSigs()[slot]));
        if (!type.has_value()) {
            Fail();
            return std::nullopt;
        }
        auto ti = type->GetTypeInfo();
        if (!ti.has_value()) {
            Fail();
            return std::nullopt;
        }
        return ti.value();
    }

    void LdTyped(AnyReg dst, uint16_t slot, uint32_t fieldId) override
    {
        UNWRAP_OPT(field, resolver.Query(Index<InstanceField>(fieldId)), Fail);
        UNWRAP_OPT(fieldOffset, field->offset, [&]() {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
        });

        ASSERT(field->refType.term.IsRecord());
        auto offset = TypedSlotOffset(slot) + fieldOffset;
        emit.LoadFrame(Ldk(field->fieldType.GetKind()), dst, offset);
    }

    void LdDerived(AnyReg dst, IReg baseRef, IReg derived, uint32_t fieldId) override
    {
        UNWRAP_OPT(field, resolver.Query(Index<InstanceField>(fieldId)), Fail);
        UNWRAP_OPT(fieldOffset, field->offset, [&]() {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
        });

        emit.LoadDerived(Ldk(field->fieldType.GetKind()), dst, baseRef, derived, fieldOffset);
    }

    void LdGeneric(IReg dst, IReg baseRef, IReg derived, IReg ti, uint32_t fieldId) override
    {
        UNWRAP_OPT(field, resolver.Query(Index<InstanceField>(fieldId)), Fail);
        UNWRAP_OPT(offset, field->offset, Fail);
        ASSERT(field->fieldType.GetKind() == CbcTypeKind::VOID);
        ASSERT(offset == 0);
        emit.LoadGeneric(dst, baseRef, derived, ti);
    }

    void Lea(IReg dst, IReg base, uint32_t fieldId) override
    {
        UNWRAP_OPT(field, resolver.Query(Index<InstanceField>(fieldId)), Fail);
        UNWRAP_OPT(offset, field->offset, [&]() {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
        });
        if (field->refType.term.IsRecord()) {
            emit.LoadRec(LDK::LD_LEA, dst, base, offset);
        } else {
            emit.LoadObj(LDK::LD_LEA, dst, base, offset);
        }
    }

    void LeaStatic(IReg dst, IReg dstBaseRef, uint32_t fieldId) override
    {
        UNWRAP_OPT(field, resolver.Query(Index<StaticField>(fieldId)), Fail);
        UNWRAP_OPT(location, field->location, Fail);
        auto symbol = emit.NewAddressSym(location);
        emit.LoadStatic(LDK::LD_LEA, dst, symbol);
        emit.MovImm(Format::Width::W64, dstBaseRef, RTSupport::Execution::GetGlobalBasePtr().value);
    }

    void LeaGeneric(IReg dst, IReg base, IReg ti, uint32_t fieldId) override
    {
        UNWRAP_OPT(field, resolver.Query(Index<InstanceField>(fieldId)), Fail);
        UNWRAP_OPT(ordinal, field->ordinal, Fail);
        emit.LeaGeneric(dst, base, ti, ordinal);
        if (field->refType.GetKind() == CbcTypeKind::REF) {
            emit.AddI(Format::Width::W64, dst, dst, RTSupport::MetaInfo::ObjectHeaderSize());
        }
    }

    void St(AnyReg src, IReg base, uint32_t fieldId) override
    {
        UNWRAP_OPT(field, resolver.Query(Index<InstanceField>(fieldId)), Fail);
        UNWRAP_OPT(offset, field->offset, [&]() {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
        });
        auto kind = Stk(field->fieldType.GetKind());
        if (field->refType.term.IsRecord()) {
            emit.StoreRec(kind, src, base, offset);
        } else {
            emit.StoreObj(kind, src, base, offset);
        }
    }

    void StStatic(AnyReg src, uint32_t fieldId) override
    {
        UNWRAP_OPT(field, resolver.Query(Index<StaticField>(fieldId)), Fail);
        UNWRAP_OPT(location, field->location, Fail);
        auto symbol = emit.NewAddressSym(location);
        emit.StoreStatic(Stk(field->fieldType.GetKind()), src, symbol);
    }

    void StTyped(AnyReg src, uint16_t slot, uint32_t fieldId) override
    {
        UNWRAP_OPT(field, resolver.Query(Index<InstanceField>(fieldId)), Fail);
        UNWRAP_OPT(fieldOffset, field->offset, [&]() {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
        });

        auto offset = TypedSlotOffset(slot) + fieldOffset;
        emit.StoreFrame(Stk(field->fieldType.GetKind()), src, offset);
    }

    void StDerived(AnyReg src, IReg baseRef, IReg derived, uint32_t fieldId) override
    {
        UNWRAP_OPT(field, resolver.Query(Index<InstanceField>(fieldId)), Fail);
        UNWRAP_OPT(fieldOffset, field->offset, [&]() {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
        });

        emit.StoreDerived(Stk(field->fieldType.GetKind()), src, baseRef, derived, fieldOffset);
    }

    void StGeneric(IReg src, IReg baseRef, IReg derived, IReg ti, uint32_t fieldId) override
    {
        UNWRAP_OPT(field, resolver.Query(Index<InstanceField>(fieldId)), Fail);
        UNWRAP_OPT(offset, field->offset, Fail);
        ASSERT(field->fieldType.GetKind() == CbcTypeKind::VOID);
        emit.StoreGeneric(src, baseRef, derived, ti, offset);
    }

    void LeaBox(IReg dst, IReg base) override
    {
        emit.LoadObj(LDK::LD_LEA, dst, base, RTSupport::MetaInfo::ObjectHeaderSize());
    }

    void Copy(IReg dstBase, IReg dst, IReg srcBase, IReg src, uint32_t typeId) override
    {
        UNWRAP_OPT(type, resolver.Query(Index<Type>(typeId)), Fail);
        UNWRAP_OPT(typeInfo, type.GetTypeInfo(), [&]() {
            errStream << "Failed to get typeinfo for type " << typeId << Stream::endl;
            Fail();
        });
        emit.CopyDerived(dstBase, dst, srcBase, src, typeInfo);
    }

    void CopyGeneric(IReg dstBase, IReg dst, IReg srcBase, IReg src, IReg ti) override
    {
        emit.CopyDerivedGeneric(dstBase, dst, srcBase, src, ti);
    }

    void CopyLocal(IReg dst, IReg src, uint32_t typeId) override
    {
        UNWRAP_OPT(type, resolver.Query(Index<Type>(typeId)), Fail);
        UNWRAP_OPT(typeInfo, type.GetTypeInfo(), Fail);
        emit.CopyLocal(dst, src, typeInfo);
    }

    void CopyToLocal(IReg dst, IReg srcBase, IReg src, uint32_t typeId) override
    {
        UNWRAP_OPT(type, resolver.Query(Index<Type>(typeId)), Fail);
        UNWRAP_OPT(typeInfo, type.GetTypeInfo(), Fail);
        emit.CopyToLocal(dst, srcBase, src, typeInfo);
    }

    void CopyFromLocal(IReg dstBase, IReg dst, IReg src, uint32_t typeId) override
    {
        UNWRAP_OPT(type, resolver.Query(Index<Type>(typeId)), Fail);
        UNWRAP_OPT(typeInfo, type.GetTypeInfo(), Fail);
        emit.CopyFromLocal(dstBase, dst, src, typeInfo);
    }

    void CopyLocalGeneric(IReg dst, IReg src, IReg ti) override { emit.CopyLocalGeneric(dst, src, ti); }

    void CopyToLocalGeneric(IReg dst, IReg srcBase, IReg src, IReg ti) override
    {
        emit.CopyToLocalGeneric(dst, srcBase, src, ti);
    }

    void CopyFromLocalGeneric(IReg dstBase, IReg dst, IReg src, IReg ti) override
    {
        emit.CopyFromLocalGeneric(dstBase, dst, src, ti);
    }

    void CopyTypeSlot(uint16_t dst, uint16_t src) override
    {
        UNWRAP_OPT(dstTi, TypeSlotTypeInfo(dst), Fail);
        UNWRAP_OPT(srcTi, TypeSlotTypeInfo(src), Fail);
        ASSERT(dstTi.UInt() == srcTi.UInt());
        emit.CopyTypeSlot(TypedSlotOffset(dst), TypedSlotOffset(src), dstTi);
    }

    void CopyToTypeSlot(uint16_t dst, IReg srcBase, IReg src) override
    {
        UNWRAP_OPT(ti, TypeSlotTypeInfo(dst), Fail);
        emit.CopyToTypeSlot(TypedSlotOffset(dst), srcBase, src, ti);
    }

    void CopyFromTypeSlot(IReg dstBase, IReg dst, uint16_t src) override
    {
        UNWRAP_OPT(ti, TypeSlotTypeInfo(src), Fail);
        emit.CopyFromTypeSlot(dstBase, dst, TypedSlotOffset(src), ti);
    }

    void CopyTypeSlotGeneric(uint16_t dst, uint16_t src, IReg ti) override
    {
        emit.CopyTypeSlotGeneric(TypedSlotOffset(dst), TypedSlotOffset(src), ti);
    }

    void CopyToTypeSlotGeneric(uint16_t dst, IReg srcBase, IReg src, IReg ti) override
    {
        emit.CopyToTypeSlotGeneric(TypedSlotOffset(dst), srcBase, src, ti);
    }

    void CopyFromTypeSlotGeneric(IReg dstBase, IReg dst, uint16_t src, IReg ti) override
    {
        emit.CopyFromTypeSlotGeneric(dstBase, dst, TypedSlotOffset(src), ti);
    }

    void LeaIndex(IReg dst, IReg src, IReg idx, uint32_t typeId) override
    {
        UNWRAP_OPT(type, resolver.Query(Index<Type>(typeId)), Fail);
        auto kind = type.term.GetKind();
        assert(kind == Engine::TermKind::CANGJIE_ARRAY || kind == Engine::TermKind::VARRAY);
        UNWRAP_OPT(elemType, resolver.QueryElement(type), [&]() {
            errStream << "Failed to get kind for type " << typeId << Stream::endl;
            Fail();
        });
        UNWRAP_OPT(typeInfo, elemType.GetTypeInfo(), [&]() {
            errStream << "Failed to get typeinfo for type " << typeId << Stream::endl;
            Fail();
        });
        emit.LeaIndex(dst, src, idx, typeInfo, kind == Engine::TermKind::CANGJIE_ARRAY);
    }

    void LeaIndexGeneric(IReg dst, IReg src, IReg idx, IReg ti) override { emit.LeaIndexGeneric(dst, src, idx, ti); }

    void LeaConstIndexGeneric(IReg dst, IReg src, int64_t idx, IReg ti) override
    {
        emit.LeaConstIndexGeneric(dst, src, idx, ti);
    }

    void LoadStackRec(IReg r, uint16_t ts) override
    {
        emit.LoadFrame(Format::LoadAccessKind::LD_LEA, r, TypedSlotOffset(ts));
    }

    void LoadTailParam(AnyReg dst, IReg tailReg, int64_t number, Format::LoadAccessKind ldk) override
    {
        auto offset = number * Cbc::STACK_SLOT_SIZE; // TODO: support different stack param layouts (e.g. iOS)
        LoadRawMemory(dst, tailReg, offset, ldk);
    }

    void LoadRawMemory(AnyReg dst, IReg base, int64_t offset, Format::LoadAccessKind ldk) override
    {
        emit.LoadRec(ldk, dst, base, offset);
    }

    void StoreRawMemory(AnyReg src, IReg base, int64_t offset, Format::StoreAccessKind stk) override
    {
        emit.StoreRec(stk, src, base, offset);
    }

    void LoadStatic(AnyReg r, uint32_t fieldId) override
    {
        auto f = resolver.Query(Index<StaticField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();
        if (!field->location.has_value()) {
            Fail();
            return;
        }
        auto symbol = emit.NewAddressSym(field->location.value());
        emit.LoadStatic(Ldk(field->fieldType.GetKind()), r, symbol);
    }

    void StoreStatic(AnyReg r, uint32_t fieldId) override
    {
        auto f = resolver.Query(Index<StaticField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();
        if (!field->location.has_value()) {
            Fail();
            return;
        }
        auto symbol = emit.NewAddressSym(field->location.value());
        emit.StoreStatic(Stk(field->fieldType.GetKind()), r, symbol);
    }

    void LoadField(IReg rb, AnyReg rd, uint32_t fieldId) override
    {
        auto f = resolver.Query(Index<InstanceField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();
        if (field->offset.has_value()) {
            emit.LoadObj(Ldk(field->fieldType.GetKind()), rd, rb, field->offset.value());
        } else {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
        }
    }

    void StoreField(IReg rb, AnyReg rs, uint32_t fieldId) override
    {
        auto f = resolver.Query(Index<InstanceField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();
        if (field->offset.has_value()) {
            if (field->refType.GetKind() == CbcTypeKind::REF) {
                emit.StoreObj(Stk(field->fieldType.GetKind()), rs, rb, field->offset.value());
            } else {
                emit.StoreRec(Stk(field->fieldType.GetKind()), rs, rb, field->offset.value());
            }
        } else {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
        }
    }

    void LoadTypeInfoGeneric(IReg dst, uint32_t typeId) override
    {
        using namespace Engine;
        auto type = resolver.Query(Index<Type>(typeId));
        if (!type.has_value()) {
            Fail();
            return;
        }
        auto term = resolver.termManager.Globalize(type->term);
        emit.LoadGenericTypeInfo(Bits::Raw64(term));
        AdjustReg(dst, IReg::IR1);
    }

    void LoadTypeInfoSig(IReg dst, uint32_t typeId) override
    {
        auto t = resolver.Query(Index<Type>(typeId));
        if (!t.has_value()) {
            Fail();
            return;
        }
        auto ti = t->GetTypeInfo();
        if (!ti.has_value()) {
            errStream << "Failed to get type info of " << *t << Stream::endl;
            Fail();
            return;
        }
        emit.MovImm(Format::Width::W64, dst, ti->UInt());
    }

    void Offset(IReg dst, IReg ti, uint32_t fieldId, bool accumulate) override
    {
        auto f = resolver.Query(Index<InstanceField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();
        if (!field->ordinal.has_value()) {
            Fail();
            return;
        }

        // TODO: one instruction
        if (accumulate) {
            emit.Offset(IReg::IR_ACC, field->ordinal.value(), ti);
            emit.Add(Format::Width::W64, dst, dst, IReg::IR_ACC);
        } else {
            emit.Offset(dst, field->ordinal.value(), ti);
        }

        if (field->refType.GetKind() == CbcTypeKind::REF) {
            emit.AddI(Format::Width::W64, dst, dst, RTSupport::MetaInfo::ObjectHeaderSize());
        }
    }

    void TagGeneric(IReg dst, IReg src, IReg ti, uint32_t typeId) override
    {
        auto t = resolver.Query(Index<Type>(typeId));
        if (!t.has_value()) {
            return Fail("resolution failure");
        }

        auto type      = *t;
        auto typeDefId = Engine::ExtractTypeDefIdentifier(type.term);
        auto typeDef   = Decode::Read(session, typeDefId);

        auto refPath = emit.NewLabel();
        auto end     = emit.NewLabel();
        emit.BranchIfRef(ti, refPath);
        emit.LoadObj(LDK::LD_U8, dst, src, RTSupport::MetaInfo::ObjectHeaderSize());
        emit.Jmp(end);
        emit.Bind(refPath);

        emit.LoadObj(LDK::LD_REF, dst, src, RTSupport::MetaInfo::ObjectHeaderSize());
        switch (typeDef->enumKind) {
            case Image::EnumKind::OPTION0: // enum { Some(T), None }
                emit.SCC(Format::CC::REQ, Format::Width::W64, dst, dst, IReg::IRZ);
                break;
            case Image::EnumKind::OPTION1: // enum { None, Some(T) }
                emit.SCC(Format::CC::RNE, Format::Width::W64, dst, dst, IReg::IRZ);
                break;
            default: return Fail("unexpected enum kind");
        }

        emit.Bind(end);
    }

    void PayloadGeneric(IReg dst, IReg src, IReg underlyingTypeInfo, IReg optionTypeInfo, uint32_t optionTypeInfoId)
        override
    {
        auto t = resolver.Query(Index<Type>(optionTypeInfoId));
        if (!t.has_value()) {
            return Fail("resolution failure");
        }

        emit.LeaGeneric(IReg::IR_ACC, src, optionTypeInfo, 1);
        emit.AddI(Format::Width::W64, IReg::IR_ACC, IReg::IR_ACC, RTSupport::MetaInfo::ObjectHeaderSize());
        emit.LoadGeneric(dst, src, IReg::IR_ACC, underlyingTypeInfo);
        BindStatePoint();
    }

    void NewNoneGeneric(IReg dst, IReg underlyingTypeInfo, IReg optionTypeInfo, uint32_t optionTypeInfoId) override
    {
        auto t = resolver.Query(Index<Type>(optionTypeInfoId));
        if (!t.has_value()) {
            return Fail("resolution failure");
        }
        auto type      = *t;
        auto typeDefId = Engine::ExtractTypeDefIdentifier(type.term);
        auto typeDef   = Decode::Read(session, typeDefId);

        emit.NewObjGenericOnAcc(optionTypeInfo);
        BindStatePoint();
        if (typeDef->enumKind == Image::EnumKind::OPTION0) {
            emit.InitNoneGenericOpt0(underlyingTypeInfo);
        }
        emit.Mov(dst, IReg::IR_ACC);
    }

    void NewSomeGeneric(IReg dst, IReg src, IReg underlyingTypeInfo, IReg optionTypeInfo, uint32_t optionTypeInfoId)
        override
    {
        auto t = resolver.Query(Index<Type>(optionTypeInfoId));
        if (!t.has_value()) {
            return Fail("resolution failure");
        }

        auto type      = *t;
        auto typeDefId = Engine::ExtractTypeDefIdentifier(type.term);
        auto typeDef   = Decode::Read(session, typeDefId);
        emit.NewObjGenericOnAcc(optionTypeInfo);
        BindStatePoint();
        emit.InitSomeGeneric(src, underlyingTypeInfo, typeDef->enumKind == Image::EnumKind::OPTION0);
        emit.Mov(dst, IReg::IR_ACC);
    }

    void AssignGeneric(IReg dst, IReg src, IReg ti) override { emit.AssignGeneric(dst, src, ti); }

    void InstanceOfGeneric(IReg dst, IReg obj, IReg ti) override { emit.InstanceOfGeneric(dst, obj, ti); }

    void AtomicLoad(IReg dst, IReg obj, uint32_t fieldId) override
    {
        auto f = resolver.Query(Index<InstanceField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();
        if (!field->offset.has_value()) {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
            return;
        }
        auto ldk = Ldk(field->fieldType.GetKind());
        emit.AtomicLoad(dst, ldk, obj, field->offset.value());
    }

    void AtomicStore(IReg src, IReg obj, uint32_t fieldId) override
    {
        auto f = resolver.Query(Index<InstanceField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();
        if (!field->offset.has_value()) {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
            return;
        }
        auto stk = Stk(field->fieldType.GetKind());
        emit.AtomicStore(src, stk, obj, field->offset.value());
    }

    void CAS(IReg dst, IReg obj, IReg expected, IReg newVal, uint32_t fieldId) override
    {
        auto f = resolver.Query(Index<InstanceField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();
        if (!field->offset.has_value()) {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
            return;
        }
        auto stk = Stk(field->fieldType.GetKind());
        RT::Opcode opc;
        switch (stk) {
            case Format::StoreAccessKind::ST_8:   opc = RT::Opcode::CAS_8; break;
            case Format::StoreAccessKind::ST_16:  opc = RT::Opcode::CAS_16; break;
            case Format::StoreAccessKind::ST_32:  opc = RT::Opcode::CAS_32; break;
            case Format::StoreAccessKind::ST_64:  opc = RT::Opcode::CAS_64; break;
            case Format::StoreAccessKind::ST_REF: opc = RT::Opcode::CAS_REF; break;
            default:                              FATAL("unexpected kind %d", stk);
        }
        emit.CAS(opc, dst, obj, expected, newVal, field->offset.value());
    }

    void AtomicSwap(IReg dst, IReg obj, IReg src, uint32_t fieldId) override
    {
        auto f = resolver.Query(Index<InstanceField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();
        if (!field->offset.has_value()) {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
            return;
        }
        auto stk = Stk(field->fieldType.GetKind());
        RT::Opcode opc;
        switch (stk) {
            case Format::StoreAccessKind::ST_8:   opc = RT::Opcode::ATOMIC_SWAP_8; break;
            case Format::StoreAccessKind::ST_16:  opc = RT::Opcode::ATOMIC_SWAP_16; break;
            case Format::StoreAccessKind::ST_32:  opc = RT::Opcode::ATOMIC_SWAP_32; break;
            case Format::StoreAccessKind::ST_64:  opc = RT::Opcode::ATOMIC_SWAP_64; break;
            case Format::StoreAccessKind::ST_REF: opc = RT::Opcode::ATOMIC_SWAP_REF; break;
            default:                              FATAL("unexpected kind %d", stk);
        }
        emit.AtomicOp(opc, dst, obj, src, field->offset.value());
    }

    void AtomicFetchAdd(IReg dst, IReg obj, IReg src, uint32_t fieldId) override
    {
        auto f = resolver.Query(Index<InstanceField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();
        if (!field->offset.has_value()) {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
            return;
        }
        auto stk = Stk(field->fieldType.GetKind());
        RT::Opcode opc;
        switch (stk) {
            case Format::StoreAccessKind::ST_8:  opc = RT::Opcode::ATOMIC_FETCH_ADD_8; break;
            case Format::StoreAccessKind::ST_16: opc = RT::Opcode::ATOMIC_FETCH_ADD_16; break;
            case Format::StoreAccessKind::ST_32: opc = RT::Opcode::ATOMIC_FETCH_ADD_32; break;
            case Format::StoreAccessKind::ST_64: opc = RT::Opcode::ATOMIC_FETCH_ADD_64; break;
            default:                             FATAL("unexpected kind %d", stk);
        }
        emit.AtomicOp(opc, dst, obj, src, field->offset.value());
    }

    void AtomicFetchSub(IReg dst, IReg obj, IReg src, uint32_t fieldId) override
    {
        auto f = resolver.Query(Index<InstanceField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();
        if (!field->offset.has_value()) {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
            return;
        }
        auto stk = Stk(field->fieldType.GetKind());
        RT::Opcode opc;
        switch (stk) {
            case Format::StoreAccessKind::ST_8:  opc = RT::Opcode::ATOMIC_FETCH_SUB_8; break;
            case Format::StoreAccessKind::ST_16: opc = RT::Opcode::ATOMIC_FETCH_SUB_16; break;
            case Format::StoreAccessKind::ST_32: opc = RT::Opcode::ATOMIC_FETCH_SUB_32; break;
            case Format::StoreAccessKind::ST_64: opc = RT::Opcode::ATOMIC_FETCH_SUB_64; break;
            default:                             FATAL("unexpected kind %d", stk);
        }
        emit.AtomicOp(opc, dst, obj, src, field->offset.value());
    }

    void AtomicFetchAnd(IReg dst, IReg obj, IReg src, uint32_t fieldId) override
    {
        auto f = resolver.Query(Index<InstanceField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();
        if (!field->offset.has_value()) {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
            return;
        }
        auto stk = Stk(field->fieldType.GetKind());
        RT::Opcode opc;
        switch (stk) {
            case Format::StoreAccessKind::ST_8:  opc = RT::Opcode::ATOMIC_FETCH_AND_8; break;
            case Format::StoreAccessKind::ST_16: opc = RT::Opcode::ATOMIC_FETCH_AND_16; break;
            case Format::StoreAccessKind::ST_32: opc = RT::Opcode::ATOMIC_FETCH_AND_32; break;
            case Format::StoreAccessKind::ST_64: opc = RT::Opcode::ATOMIC_FETCH_AND_64; break;
            default:                             FATAL("unexpected kind %d", stk);
        }
        emit.AtomicOp(opc, dst, obj, src, field->offset.value());
    }

    void AtomicFetchOr(IReg dst, IReg obj, IReg src, uint32_t fieldId) override
    {
        auto f = resolver.Query(Index<InstanceField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();
        if (!field->offset.has_value()) {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
            return;
        }
        auto stk = Stk(field->fieldType.GetKind());
        RT::Opcode opc;
        switch (stk) {
            case Format::StoreAccessKind::ST_8:  opc = RT::Opcode::ATOMIC_FETCH_OR_8; break;
            case Format::StoreAccessKind::ST_16: opc = RT::Opcode::ATOMIC_FETCH_OR_16; break;
            case Format::StoreAccessKind::ST_32: opc = RT::Opcode::ATOMIC_FETCH_OR_32; break;
            case Format::StoreAccessKind::ST_64: opc = RT::Opcode::ATOMIC_FETCH_OR_64; break;
            default:                             FATAL("unexpected kind %d", stk);
        }
        emit.AtomicOp(opc, dst, obj, src, field->offset.value());
    }

    void AtomicFetchXor(IReg dst, IReg obj, IReg src, uint32_t fieldId) override
    {
        auto f = resolver.Query(Index<InstanceField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();
        if (!field->offset.has_value()) {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
            return;
        }
        auto stk = Stk(field->fieldType.GetKind());
        RT::Opcode opc;
        switch (stk) {
            case Format::StoreAccessKind::ST_8:  opc = RT::Opcode::ATOMIC_FETCH_XOR_8; break;
            case Format::StoreAccessKind::ST_16: opc = RT::Opcode::ATOMIC_FETCH_XOR_16; break;
            case Format::StoreAccessKind::ST_32: opc = RT::Opcode::ATOMIC_FETCH_XOR_32; break;
            case Format::StoreAccessKind::ST_64: opc = RT::Opcode::ATOMIC_FETCH_XOR_64; break;
            default:                             FATAL("unexpected kind %d", stk);
        }
        emit.AtomicOp(opc, dst, obj, src, field->offset.value());
    }

    bool IsFuture(const Type& type)
    {
        if (type.term.GetKind() != Engine::TermKind::TYPE) {
            return false;
        }

        auto identifier = Engine::ExtractTypeDefIdentifier(type.term);
        auto def        = Decode::Read(session, identifier);
        return Decode::Read(session, def.GetName()).compare("std.core:Future") == 0;
    }

    std::optional<Type> NewObject(IReg dst, uint32_t typeId, New kind)
    {
        auto t = resolver.Query(Index<Type>(typeId));
        if (!t.has_value()) {
            Fail();
            return std::nullopt;
        }
        auto ti = t->GetTypeInfo();
        if (!ti.has_value()) {
            errStream << "Failed to get type info of " << *t << Stream::endl;
            Fail();
            return std::nullopt;
        }

        if (kind == New::Obj && IsFuture(*t)) {
            kind = New::ObjPinned;
        }

        auto typeInfo = *ti;
        switch (kind) {
            case New::Obj:       emit.NewObj(typeInfo, false); break;
            case New::Arr:       emit.NewArr(typeInfo); break;
            case New::ObjPinned: emit.NewObj(typeInfo, true); break;
        }
        BindStatePoint();
        AdjustReg(dst, IReg::IR1);
        return *t;
    }

    void NewObj(IReg dst, uint32_t typeId) override { NewObject(dst, typeId, New::Obj); }

    void LeaCForeign(IReg dst, uint32_t methodId) override
    {
        UNWRAP_OPT(method, resolver.Query(Index<DirectCall>(methodId)), Fail);
        if (auto data = std::get_if<DirectCall::Compiled>(&method->data)) {
            emit.MovImm(Format::Width::W64, dst, data->funcPtr);
        } else {
            Fail();
        }
    }

    void CallCFunc(IReg target, uint32_t methodId, Utils::Span<const Location> args) override
    {
        auto ref = Decode::Read(
            session,
            Image::RefIdentifier<Image::MethodReference>(Image::RefId<Image::MethodReference>(methodId), fileId)
        );
        auto sig   = Engine::TermManager::Resolve(session, ref.methodSig);
        auto tvars = Engine::TermManager::Resolve(session, ref.tvars);
        ASSERT(tvars.GetLength() == 0);
        Resolution::MethodSignature methodSig = { .resolver = &resolver,
                                                  .term     = sig,
                                                  .tvars    = tvars }; // synthetic method signature
        auto [stackSlotsCount, fregsCount] =
            StageForeignCallArgs(args, MakeCallAbiFlags(ref.flags, ref.abiKind), methodSig);
        ASSERTION(stackSlotsCount * STACK_SLOT_SIZE <= UINT8_MAX, "too many stack arguments in foreign call");
        emit.CallCFunc(target, stackSlotsCount * STACK_SLOT_SIZE, fregsCount);
        EmitFillSRetAreaByRegs();
        BindStatePoint();
        EmitReturnedTo();
    }

    CallAbiFlags MakeCallAbiFlags(Image::MethodRefFlags flags, Image::MethodAbiKind abiKind) const
    {
        return { .isSRet     = flags.Is(Image::MethodRefFlag::SRET),
                 .hasOuterTi = flags.Is(Image::MethodRefFlag::HAS_OUTER_TI),
                 .abiKind    = abiKind };
    }

    void CallDirect(uint32_t methodId, Utils::Span<const Location> args) override
    {
        UNWRAP_OPT(method, resolver.Query(Index<DirectCall>(methodId)), Fail);

        auto ref = Decode::Read(
            session,
            Image::RefIdentifier<Image::MethodReference>(Image::RefId<Image::MethodReference>(methodId), fileId)
        );
        auto compiled                      = std::get_if<DirectCall::Compiled>(&method->data);
        bool isForeign                     = compiled != nullptr && compiled->isForeign;
        auto callFlags                     = MakeCallAbiFlags(ref.flags, ref.abiKind);
        auto [stackSlotsCount, fregsCount] = isForeign ? StageForeignCallArgs(args, callFlags, method->signature)
                                                       : StageManagedCallArgs(args, callFlags, method->signature);

        if (compiled) {
            auto sym = emit.NewAddressSym(compiled->funcPtr);
            if (compiled->isForeign) {
                EmitLogCall("call.2n", method);
                ASSERTION(stackSlotsCount * STACK_SLOT_SIZE <= UINT8_MAX, "too many stack arguments in foreign call");
                emit.DirectCall2n(sym, stackSlotsCount * STACK_SLOT_SIZE, fregsCount);
                EmitFillSRetAreaByRegs();
            } else {
                EmitLogCall("call.2c", method);
                emit.DirectCall2c(sym);
            }
        } else {
            EmitLogCall("call.2i", method);
            auto fuh = std::get<Interpretation::DynamicFunctionHandle*>(method->data);
            auto sym = emit.NewAddressSym(reinterpret_cast<uintptr_t>(fuh));
            emit.DirectCall2i(sym);
        }
        BindStatePoint();
        EmitReturnedTo();
    }

    void CallVirtual(uint32_t methodId, Utils::Span<const Location> args) override
    {
        auto m = resolver.Query(Index<VirtualCall>(methodId));
        if (!m.has_value()) {
            Fail();
            return;
        }
        auto method = m.value();
        auto ref    = Decode::Read(
            session,
            Image::RefIdentifier<Image::MethodReference>(Image::RefId<Image::MethodReference>(methodId), fileId)
        );
        StageManagedCallArgs(args, MakeCallAbiFlags(ref.flags, ref.abiKind), method->signature);
        EmitLogCall("call.virt", method);
        emit.VirtualCall(method->methodNum, method->extDefNum, method->sret);
        BindStatePoint();
        EmitReturnedTo();
    }

    static uint16_t ArgNumber(Location loc)
    {
        uint16_t argnum = 0;
        if (loc.Kind() == Location::NIL) {
            argnum = Interpretation::StaticCallTypeInfoArgs::NONE;
        } else if (loc.Kind() == Location::IREG) {
            argnum = loc.IRegIdx();
            ASSERT(argnum < IReg::VIRT_COUNT);
        } else if (loc.Kind() == Location::SLOT) {
            argnum = loc.SlotIdx() + IReg::VIRT_COUNT;
        } else {
            ASSERTION(false, "outerti loc can not be freg");
        }
        return argnum;
    }

    void CallInterf(uint32_t methodId, Utils::Span<const Location> args) override
    {
        auto m = resolver.Query(Index<InterfaceCall>(methodId));
        if (!m.has_value()) {
            Fail();
            return;
        }
        auto method = m.value();
        auto ti     = method->refType.GetTypeInfo();
        if (!ti.has_value()) {
            Fail();
            return;
        }
        auto ref = Decode::Read(
            session,
            Image::RefIdentifier<Image::MethodReference>(Image::RefId<Image::MethodReference>(methodId), fileId)
        );

        auto flags         = MakeCallAbiFlags(ref.flags, ref.abiKind);
        Location thisTiLoc = Location::Nil();
        StageManagedCallArgs(args, MakeCallAbiFlags(ref.flags, ref.abiKind), method->signature, nullptr, &thisTiLoc);
        EmitLogCall("call.interf", method);

        uint16_t thisTiLocArgNum = ArgNumber(thisTiLoc);

        Interpretation::StaticCallTypeInfoArgs tiArgs = {
            .outerTi = Interpretation::StaticCallTypeInfoArgs::NONE,
            .thisTi  = thisTiLocArgNum,
        };

        emit.InterfaceCall(method->methodNum, *ti, flags.isSRet, tiArgs);
        BindStatePoint();
        EmitReturnedTo();
    }

    void CallInterfGeneric(uint16_t _, uint32_t methodId, Utils::Span<const Location> args) override
    {
        auto m = resolver.Query(Index<InterfaceCall>(methodId));
        if (!m.has_value()) {
            Fail();
            return;
        }
        auto method = m.value();
        auto ref    = Decode::Read(
            session,
            Image::RefIdentifier<Image::MethodReference>(Image::RefId<Image::MethodReference>(methodId), fileId)
        );
        auto flags          = MakeCallAbiFlags(ref.flags, ref.abiKind);
        Location outerTiLoc = Location::Nil();
        Location thisTiLoc  = Location::Nil();
        StageManagedCallArgs(args, flags, method->signature, &outerTiLoc, &thisTiLoc);
        EmitLogCall("call.interf.g", method);

        uint16_t outerTiLocArgNum = ArgNumber(outerTiLoc);
        uint16_t thisTiLocArgNum  = ArgNumber(thisTiLoc);

        Interpretation::StaticCallTypeInfoArgs tiArgs = {
            .outerTi = outerTiLocArgNum,
            .thisTi  = thisTiLocArgNum,
        };

        EmitLogCall("call.interf.g", method);
        emit.InterfaceCallGeneric(method->methodNum, flags.isSRet, tiArgs);
        BindStatePoint();
        EmitReturnedTo();
    }

    void Spawn(IReg closure, uint32_t typeId) override
    {
        AdjustReg(IReg::IR_ACC, closure);

        auto t = resolver.QueryFutureByFunctional(Index<Type>(typeId));
        if (!t.has_value()) {
            Fail();
            return;
        }
        auto type        = t.value();
        auto optTypeInfo = type.GetTypeInfo();
        if (!optTypeInfo.has_value()) {
            Fail();
            return;
        }
        auto typeInfo = *optTypeInfo;
        emit.Spawn(typeInfo);
        BindStatePoint();
    }

    void SpawnFuture(IReg future, uint32_t /*typeId*/) override
    {
        AdjustReg(IReg::IR_ACC, future);
        emit.SpawnFuture();
        BindStatePoint();
    }

    void CallClosure(uint32_t typeId, bool generic, Utils::Span<const Location> args) override
    {
        auto t = resolver.Query(Index<Type>(typeId));
        if (!t.has_value() || t->term.GetKind() != Engine::TermKind::FUNCTIONAL) {
            return Fail("failed to resolve type");
        }
        auto term    = t->term;
        auto retType = term.Subterm(term.GetLength() - 1);

        // For instantiated version of closure `sret` can be computed
        // by retType kind.
        bool sret = (resolver.Wrap(retType).GetKind() == TK::REC) || generic;

        Resolution::MethodSignature sig { &resolver, term, Engine::Term::Predefined(Engine::TermKind::NIL) };
        StageManagedCallArgs(
            args,
            { .isSRet = sret, .hasOuterTi = true, .abiKind = Image::MethodAbiKind::REF_RECEIVER },
            sig,
            nullptr,
            nullptr,
            generic
        );

        if (generic) {
            // Generic calls of closure are always considered as `sret`.
            emit.CallClosureGeneric();
            BindStatePoint();
            return;
        }

        emit.CallClosure(sret);
        BindStatePoint();
    }

    void NewClosure(IReg dst, uint32_t typeId) override
    {
        auto type = NewObject(IReg::IR1, typeId, New::Obj); // has BindStatePoint call inside
        if (!type.has_value()) {
            return;
        }

        auto typeDefId = Engine::TypeTermId(type->term).GetIdentifier();
        auto typeDef   = Decode::Read(resolver, typeDefId);

        // - Get 1th methodId out of iterator.
        // - Emit `InitClosure` with flags of 1th method.
        // - Ensure that there are only two methods in the closure type.
        int idx = 0;
        for (auto methodId : Decode::Resolve(resolver, typeDef.GetVirtualMethods())) {
            if (idx == 1) {
                auto method = Decode::Read(resolver, methodId);
                auto sret   = method.GetFlags().Is(Image::MethodFlag::SRET);

                emit.InitClosure(sret);
                AdjustReg(dst, IReg::IR1);
            }
            idx++;
        }
        if (idx != 2) {
            Fail("failed to find instantiated version of method in closure");
        }
    }

    void NewObjGeneric(IReg ti, uint32_t typeId) override
    {
        auto type = resolver.Query(Index<Type>(typeId));
        if (!type.has_value()) {
            Fail("failed to resolve generic object type");
            return;
        }

        emit.NewObjGeneric(ti, IsFuture(*type));
        BindStatePoint();
    }

    void NewClosureGeneric(IReg ti, uint32_t typeId) override
    {
        auto type = resolver.Query(Index<Type>(typeId));
        if (!type.has_value()) {
            return;
        }

        emit.NewObjGeneric(ti);
        BindStatePoint();
        emit.InitClosure(true); // generic closures are always SRET
    }

    void Scc(Format::Width width, Format::CC cc, IReg d, AnyReg l, AnyReg r) override
    {
        if (cc.IsFloatingPoint()) {
            emit.SCC(cc, width, d, FReg::From(l), FReg::From(r));
        } else {
            emit.SCC(cc, width, d, IReg::From(l), IReg::From(r));
        }
    }

    void SccImm(Format::Width width, Format::CC cc, IReg d, IReg l, uint64_t imm) override
    {
        emit.SCCImm(cc, width, d, l, imm);
    }

    void Ret(Format::Width width, IReg src) override
    {
        AdjustReg(IReg::IR1, src);
        emit.Ret();
    }

    void FRet(Format::Width width, FReg src) override
    {
        if (src != FReg::FR0) {
            emit.Mov(FReg::FR0, src);
        }
        emit.Ret();
    }

    void RetRef(IReg src) override
    {
        AdjustReg(IReg::IR1, src);
        emit.Ret();
    }

    void DivCheck(IReg reg) override
    {
        emit.DivCheck(reg);
        BindStatePoint();
    }

    void NullCheck(IReg reg) override
    {
        emit.NullCheck(reg);
        BindStatePoint();
    }

    void Catch(IReg reg) override { emit.Catch(reg); }

    void Throw(IReg reg) override { emit.Throw(reg); }

    void InstanceOf(IReg dst, IReg obj, uint32_t typeId) override
    {
        auto t = resolver.Query(Index<Type>(typeId));
        if (!t.has_value()) {
            Fail();
            return;
        }
        auto type = t.value();
        if (!type.GetTypeInfo().has_value()) {
            errStream << "Failed to get type info of " << type << Stream::endl;
            Fail();
            return;
        }

        auto typeInfo = type.GetTypeInfo().value();
        emit.InstanceOf(dst, obj, typeInfo);
    }

    void LoadTypeInfoObj(IReg dst, IReg obj) override { emit.LoadObj(Format::LoadAccessKind::LD_64, dst, obj, 0); }

    void InitObj(uint16_t ts) override { FATAL("not implemented"); }

    void InitString(uint16_t ts, uint32_t offset) override
    {
        auto str = resolver.QueryString(offset);
        // FIXME: string intern!
        if (str.size() > UINT32_MAX) {
            // TODO: log
            Fail();
        }
        // TODO: allocate proper array in heap?
        size_t size = str.size();
        auto mem    = std::malloc(sizeof(Interpretation::StringStorage) + size + 1);
        if (!mem) {
            FATAL("out of memory"); // FIXME: rewrite to throwing stub
        }

        auto storage = new (mem) Interpretation::StringStorage { RTSupport::MetaInfo::ByteArrayTypeInfo(), size };
        std::memcpy(storage->string, str.data(), size);
        storage->string[size] = 0;
        emit.StringLit(storage, TypedSlotOffset(ts));
    }

    void ArrayLength(IReg dst, IReg arr) override
    {
        emit.LoadObj(Format::LoadAccessKind::LD_64, dst, arr, RTSupport::MetaInfo::ObjectHeaderSize());
    }

    void ArrayIndexCheck(IReg length, IReg index) override { FATAL("not implemented"); }

    int32_t UntypedSlotOffset(uint16_t us) { return frameLayout.UntypedSlotOffset(us); }

    void LoadUntyped(AnyReg dst, Format::LoadAccessKind ldk, uint16_t us) override
    {
        emit.LoadFrame(ldk, dst, UntypedSlotOffset(us));
    }

    void StoreUntyped(AnyReg src, Format::StoreAccessKind stk, uint16_t us) override
    {
        emit.StoreFrame(stk, src, UntypedSlotOffset(us));
    }

    void StoreUntypedImm(uint64_t imm, uint16_t us) override
    {
        emit.StoreFrameImm(Format::StoreAccessKind::ST_64, imm, UntypedSlotOffset(us));
    }

    void LoadTyped(AnyReg dst, uint16_t ts, uint16_t fieldId) override
    {
        auto f = resolver.Query(Index<InstanceField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();
        if (field->offset.has_value()) {
            auto offset = TypedSlotOffset(ts) + field->offset.value();
            emit.LoadFrame(Ldk(field->fieldType.GetKind()), dst, offset);
        } else {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
        }
    }

    void StoreTyped(AnyReg src, uint16_t ts, uint16_t fieldId) override
    {
        auto f = resolver.Query(Index<InstanceField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();
        if (field->offset.has_value()) {
            auto offset = TypedSlotOffset(ts) + field->offset.value();
            emit.StoreFrame(Stk(field->fieldType.GetKind()), src, offset);
        } else {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
        }
    }

    void StoreTypedImm(uint64_t imm, uint16_t ts, uint16_t fieldId) override
    {
        auto f = resolver.Query(Index<InstanceField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();
        if (field->offset.has_value()) {
            auto offset = TypedSlotOffset(ts) + field->offset.value();
            emit.StoreFrameImm(Stk(field->fieldType.GetKind()), imm, offset);
        } else {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
        }
    }

    void LoadArray(AnyReg dst, Format::LoadAccessKind ldk, IReg arr, IReg idx) override
    {
        emit.LoadArray(ldk, dst, arr, idx);
    }

    void StoreArray(AnyReg src, Format::StoreAccessKind stk, IReg arr, IReg idx) override
    {
        emit.StoreArray(stk, src, arr, idx);
    }

    void TypeArg(IReg ti, int idx, IReg dst) override { emit.TypeArg(dst, ti, idx); }

    Interpretation::BuiltinType ToBuiltin(Engine::TermKind tk)
    {
        using namespace Interpretation;
        switch (tk) {
            case Engine::TermKind::UNIT:    return BUILTIN_UNIT;
            case Engine::TermKind::BOOLEAN: return BUILTIN_BOOLEAN;
            case Engine::TermKind::U8:      return BUILTIN_U8;
            case Engine::TermKind::I8:      return BUILTIN_I8;
            case Engine::TermKind::U16:     return BUILTIN_U16;
            case Engine::TermKind::I16:     return BUILTIN_I16;
            case Engine::TermKind::U32:     return BUILTIN_U32;
            case Engine::TermKind::I32:     return BUILTIN_I32;
            case Engine::TermKind::U64:     return BUILTIN_U64;
            case Engine::TermKind::I64:     return BUILTIN_I64;
            case Engine::TermKind::UADDR:   return BUILTIN_UADDR;
            case Engine::TermKind::IADDR:   return BUILTIN_IADDR;
            case Engine::TermKind::F16:     return BUILTIN_F16;
            case Engine::TermKind::F32:     return BUILTIN_F32;
            case Engine::TermKind::F64:     return BUILTIN_F64;
            case Engine::TermKind::UCHAR32: return BUILTIN_RUNE;
            case Engine::TermKind::BSTRING: return BUILTIN_CSTRING;
            case Engine::TermKind::NOTHING: return BUILTIN_NOTHING;

            default: Fail("unexpected builtin kind"); return Interpretation::BUILTIN_I64;
        }
    }

    void Box(AnyReg src, IReg dst, uint32_t type) override
    {
        if (type < Engine::Term::FIRST_NON_PRIMITIVE) {
            auto tk = Engine::TermKind(type);
            auto bt = ToBuiltin(tk);
            emit.NewBox(bt); // Spoils IR_ACC
            BindStatePoint();
            // Unit's box contains only the object header.
            if (bt != Interpretation::BUILTIN_UNIT) {
                emit.StoreObj(Stk(bt), src, IReg::IR_ACC, RTSupport::MetaInfo::ObjectHeaderSize());
            }
            AdjustReg(dst, IReg::IR_ACC);
        } else {
            auto t = resolver.Query(Index<Type>(type));
            if (!t.has_value()) {
                Fail();
                return;
            }
            auto type = t.value();
            auto ti   = type.GetTypeInfo();
            if (!ti.has_value()) {
                Fail();
                return;
            }
            emit.NewBox(*ti); // Spoils IR_ACC
            BindStatePoint();
            auto kind = type.GetKind();
            switch (kind) {
                case CbcTypeKind::REC:
                    emit.WriteStructField(IReg::From(src), IReg::IR_ACC, RTSupport::MetaInfo::ObjectHeaderSize(), *ti);
                    break;
                default: emit.StoreObj(Stk(kind), src, IReg::IR_ACC, RTSupport::MetaInfo::ObjectHeaderSize()); break;
            }
            AdjustReg(dst, IReg::IR_ACC);
        }
    }

    void BoxGeneric(IReg src, IReg dst, IReg ti) override
    {
        emit.NewObjGenericOnAcc(ti);
        BindStatePoint();
        emit.StoreGeneric(src, IReg::IR_ACC, IReg::IR_ACC, ti, RTSupport::MetaInfo::ObjectHeaderSize());
        AdjustReg(dst, IReg::IR_ACC);
    }

    void BoxT(uint16_t srcTs, IReg dst) override
    {
        UNWRAP_OPT(typeInfo, TypeSlotTypeInfo(srcTs), Fail);
        auto offset = TypedSlotOffset(srcTs);
        emit.NewBox(typeInfo);
        BindStatePoint();
        AdjustReg(dst, IReg::IR_ACC);

        emit.LoadFrame(Format::LoadAccessKind::LD_LEA, IReg::IR_ACC, offset);
        emit.WriteStructField(IReg::IR_ACC, dst, RTSupport::MetaInfo::ObjectHeaderSize(), typeInfo);
    }

    void BoxTGeneric(uint16_t srcTs, IReg dst, IReg ti) override
    {
        const auto offset = TypedSlotOffset(srcTs);
        emit.LoadFrame(Format::LoadAccessKind::LD_LEA, IReg::IR_ACC, offset);
        emit.LoadGeneric(dst, IReg::IRZ, IReg::IR_ACC, ti);
        BindStatePoint();
    }

    void Unbox(AnyReg dst, IReg src, uint32_t type) override
    {
        if (type < Engine::Term::FIRST_NON_PRIMITIVE) {
            auto tk = Engine::TermKind(type);
            auto bt = ToBuiltin(tk);
            if (bt == Interpretation::BUILTIN_UNIT) {
                AdjustReg(IReg::From(dst), IReg::IRZ);
            } else {
                emit.LoadObj(Ldk(bt), dst, src, RTSupport::MetaInfo::ObjectHeaderSize());
            }
        } else {
            auto t = resolver.Query(Index<Type>(type));
            if (!t.has_value()) {
                Fail();
                return;
            }
            auto type = t.value();
            auto kind = type.GetKind();
            switch (kind) {
                case CbcTypeKind::REF:
                    emit.LoadObj(Format::LoadAccessKind::LD_REF, dst, src, RTSupport::MetaInfo::ObjectHeaderSize());
                    break;
                case CbcTypeKind::REC: {
                    auto ti = type.GetTypeInfo();
                    if (!ti.has_value()) {
                        Fail();
                        return;
                    }
                    auto typeInfo = ti.value();
                    emit.ReadStructField(IReg::From(dst), src, RTSupport::MetaInfo::ObjectHeaderSize(), typeInfo);
                    break;
                }
                default: emit.LoadObj(Ldk(kind), dst, src, RTSupport::MetaInfo::ObjectHeaderSize()); break;
            }
        }
    }

    void UnboxT(uint16_t dstTs, IReg src) override
    {
        auto offset = TypedSlotOffset(dstTs);
        emit.UnboxT(src, offset);
    }

    enum HeadKind {
        HEAD_NONE,
        HEAD_OBJ,
        HEAD_REC,
        HEAD_DERIVED,
        HEAD_STATIC,
        HEAD_FRAME
    };

    void ParseOne() override
    {
        auto position = Pos();
        startPosition = position;
        emit.Bind(InstructionLabel(position));
        IsaParser::ParseOne();
    }

    void End() override
    {
        emit.Bind(InstructionLabel(Pos()));
        IsaParser::End();
    }

    void Fail(std::string&& msg = "") { failureMessages.PushBack({ startPosition, std::move(msg) }); }

    void StopRewrite()
    {
        Fail();
        auto left = reader.End() - reader.Cursor();
        reader.Advance(left);
    }
};

static std::optional<FrameLayout> MakeFrameLayout(Image::Code code, Resolver& resolver)
{
    auto& log = Interpretation::Log::preparation;

    auto savedRegsCount = 0;
    for (uint8_t i = 0, savedRegs = code.UsedNonVolIRegMask(); i < (IReg::COUNT - IReg::FIRST_NON_VOL); i++) {
        if ((savedRegs & (1 << i)) != 0)
            savedRegsCount++;
    }
    for (uint8_t i = 0, savedRegs = code.UsedNonVolFRegMask(); i < (FReg::COUNT - FReg::FIRST_NON_VOL); i++) {
        if ((savedRegs & (1 << i)) != 0)
            savedRegsCount++;
    }
    auto savedRegsSpace = Cbc::STACK_SLOT_SIZE * savedRegsCount;

    auto untypedSlotsSize = Cbc::STACK_SLOT_SIZE * code.UntypedSlotCount();

    Utils::Vector<uint32_t> typedOffset;
    typedOffset.Resize(code.StackAllocSigsCount() + 1);

    Utils::Vector<uint32_t> refOffsets;

    auto stackAllocSize = untypedSlotsSize;
    uint32_t i;
    for (i = 0; i < code.StackAllocSigsCount(); i++) {
        auto typeOpt = resolver.Query(Index<Type>(code.StackAllocSigs()[i]));
        if (!typeOpt.has_value()) {
            LOG_ERROR(log, "Failed to query type at stack-alloc index {}", i);
            return std::nullopt;
        }
        auto type = typeOpt.value();
        if (type.GetKind() != CbcTypeKind::REC) {
            LOG_ERROR(log, "Unexpected kind {} at t{} for type {}", (uint8_t)type.GetKind(), i, type);
            return std::nullopt;
        }
        auto size = type.GetFlatSize();
        if (!size.has_value()) {
            LOG_ERROR(log, "Unknown size at t{} for type {}", i, type);
            return std::nullopt;
        }

        type.FillReferenceOffsets(refOffsets, stackAllocSize);
        typedOffset[i]  = stackAllocSize;
        stackAllocSize += MathUtils::AlignUp(size.value(), Cbc::STACK_SLOT_SIZE);
    }

    typedOffset[code.StackAllocSigsCount()] = stackAllocSize;
    auto frameSize                          = savedRegsSpace + stackAllocSize;
    // Transform all offsets from bottom-relative to end-of-essentials-relative (negative).

    return FrameLayout { std::move(typedOffset), std::move(refOffsets), frameSize };
}

static Utils::Vector<Interpretation::GCPositionalInfo> CalculatePositionalGCInfo(
    Engine::Session& session,
    const MethodCode& code,
    Emitter::Emitter const& emitter,
    Utils::Span<IsaRewriter::StatePoint const> statePoints,
    int32_t frameSize
)
{
    auto livenessInfo = Decode::GetLivenessInfo(session, code);

    Utils::Vector<Interpretation::GCPositionalInfo> posInfo;
    posInfo.Reserve(livenessInfo.Size());

    std::unordered_map<ssize_t, Image::LivenessInfo const&> infos;
    for (const auto& info : livenessInfo) {
        infos.insert({ info.cbcPos, info });
    }

    for (auto& point : statePoints) {
        auto originalPos  = point.originalPos;
        auto rewrittenPos = emitter.LabelPosition(point.label);
        auto it           = infos.find(originalPos);
        if (it == infos.end()) {
            continue;
        } else if (rewrittenPos > UINT32_MAX) {
            FATAL("Position too big");
        }
        auto& info = it->second;

        posInfo.PushBack({ .rewrittenPos    = (uint32_t)rewrittenPos,
                           .regMask         = info.regMask,
                           .untypedRefSlots = info.refSlotNums,
                           .paramRefSlots   = point.paramRefSlots,
                           .paramRecSlots   = point.paramRecSlots,
                           .mutPairs        = {} });

        posInfo.Back().mutPairs.Reserve(info.mutPairs.Size());
        for (const auto& pair : info.mutPairs) {
            auto mutRes = std::pair(
                Interpretation::Resource { .idx = pair.first }, Interpretation::Resource { .idx = pair.second }
            );
            posInfo.Back().mutPairs.PushBack(mutRes);
        }
    }

    return posInfo;
}

static Utils::Vector<Interpretation::StackPtrsPositionalInfo> CalculateStackPtrsPositionalInfo(
    Engine::Session& session,
    const MethodCode& code,
    Emitter::Emitter const& emitter,
    Utils::Span<IsaRewriter::StatePoint const> statePoints
)
{
    auto stackPtrsInfo = Decode::GetStackPtrsInfo(session, code);

    Utils::Vector<Interpretation::StackPtrsPositionalInfo> posInfo;
    posInfo.Reserve(stackPtrsInfo.Size());

    // FIXME: the data must be stored in the format that is compact and fast to query.
    std::unordered_map<ssize_t, Image::StackPtrsInfo const&> infos;
    for (const auto& info : stackPtrsInfo) {
        infos.insert({ info.cbcPos, info });
    }

    for (auto& point : statePoints) {
        auto originalPos  = point.originalPos;
        auto rewrittenPos = emitter.LabelPosition(point.label);
        auto it           = infos.find(originalPos);
        if (it == infos.end()) {
            // Stack ptrs info is collected for a subset of state points
            continue;
        } else if (rewrittenPos > UINT32_MAX) {
            FATAL("Position too big");
        }
        auto& info = it->second;

        Interpretation::StackPtrsPositionalInfo newInfo = { .rewrittenPos = (uint32_t)rewrittenPos, .resources = {} };

        newInfo.resources.Reserve(info.resources.Size());
        for (const auto& res : info.resources) {
            newInfo.resources.PushBack(Interpretation::Resource { .idx = res });
        }
        posInfo.EmplaceBack(std::move(newInfo));
    }

    return posInfo;
}

static std::vector<std::pair<uint32_t, uint32_t>> CalculateBcPositionsForExceptions(
    Engine::Session& session,
    const MethodCode& code,
    Emitter::Emitter const& emitter,
    Utils::Vector<IsaRewriter::StatePoint> const& statePoints
)
{
    auto sourceCodeInfo = Decode::GetSourceCodeInfo(session, code);

    std::unordered_set<ssize_t> infos;
    for (const auto& bcPos : sourceCodeInfo.cbcBcPositions) {
        infos.insert(bcPos);
    }

    std::vector<std::pair<uint32_t, uint32_t>> bcPositions;
    for (auto& point : statePoints) {
        auto originalPos  = point.originalPos;
        auto rewrittenPos = emitter.LabelPosition(point.label);
        auto it           = infos.find(originalPos);
        if (it == infos.end()) {
            continue;
        }
        if (rewrittenPos > UINT32_MAX) {
            FATAL("Position too big");
        } else {
            bcPositions.emplace_back(std::pair { rewrittenPos, originalPos });
        }
    }

    return bcPositions;
}

static std::string Descriptor(Engine::Session& session, Image::Identifier<Image::MethodDefinition> method)
{
    Stream::StringBuffer buf;
    Stream::ResolvingOutput out(session, buf);
    out << method << " ";
    return buf.ToString();
}

std::optional<Interpretation::ExecBytecodeInfo> Rewrite(
    Engine::Session& session,
    MethodCode& code,
    Resolver& resolver,
    Memory::Heap& heap,
    Image::Identifier<Image::MethodDefinition> method
)
{
    using namespace Stream;
    Emitter::Emitter emitter;
    auto frameLayout = MakeFrameLayout(code, resolver);

    if (!frameLayout.has_value()) {
        FATAL("Rewriter failed: cannot make frame layout.");
    }

    auto rewriter = IsaRewriter(resolver, session, method, code, *frameLayout, emitter);
    rewriter.ParseAll();

    auto fullFrameSize =
        MathUtils::AlignUp(frameLayout->frameSize + rewriter.maxParamPassingSize, Cbc::FRAME_ALIGNMENT);

    if (!rewriter.failureMessages.Empty()) {
        Interpretation::Log::preparation.Log(Logging::Level::ERROR, [&](Stream::Output& out) {
            out << "Failed to rewrite method at positions: ";
            for (auto failure : rewriter.failureMessages) {
                out << failure.position << ": " << failure.message << endl;
            }
        });
        return std::nullopt;
    }

    auto def     = Decode::Read(session, method);
    auto flags   = def.GetABIFlags();
    auto abiInfo = Interpretation::BuildAbiInfo(
        session,
        Engine::TermManager::Resolve(session, def.Signature()),
        {
            .isSRet     = flags.Is(Image::MethodRefFlag::SRET),
            .isMut      = def->abiKind == Image::MethodAbiKind::MUT,
            .hasOuterTi = flags.Is(Image::MethodRefFlag::HAS_OUTER_TI),
            .abiKind    = def->abiKind,
            .funcVars   = def->arity,
        }
    );

    auto rewrittenCode = emitter.Build(heap);

    return Interpretation::ExecBytecodeInfo {
        .code             = rewrittenCode,
        .savedIRegs       = Interpretation::NonVolatileRegs(code.UsedNonVolIRegMask() << IReg::FIRST_NON_VOL),
        .savedFRegs       = Interpretation::NonVolatileRegs(code.UsedNonVolFRegMask() << FReg::FIRST_NON_VOL),
        .frameSize        = fullFrameSize,
        .staticFrameSize  = frameLayout->frameSize,
        .untypedSlotCount = static_cast<uint16_t>(code.UntypedSlotCount()),
        .abiInfo          = std::move(abiInfo),
        .gcInfo =
            Interpretation::GcInfo {
                .positionalInfo =
                    CalculatePositionalGCInfo(session, code, emitter, rewriter.statePoints, frameLayout->frameSize),
                .refOffsets = std::move(frameLayout->refOffsets),
            },
        .stackPtrsInfo =
            Interpretation::StackPtrsInfo {
                .positionalInfo = CalculateStackPtrsPositionalInfo(session, code, emitter, rewriter.statePoints) },
        .offsetsIndex             = rewriter.BuildOffsetsIndex(), // TODO remove, reuse bcPositionsForExceptions
        .bcPositionsForExceptions = CalculateBcPositionsForExceptions(session, code, emitter, rewriter.statePoints),
    };
}

std::optional<Interpretation::ExecBytecodeInfo> Rewrite(
    Engine::Session& session, Image::Identifier<Image::MethodDefinition> method, Memory::Heap& heap
)
{
    using namespace Stream;
    auto def = Decode::Read(session, method);
    ASSERTION(def.MethodCode().has_value(), "fuh preparation must be unreachable for methods without code");

    Resolver resolver(session, method);
    auto code = Decode::Read(session, def.MethodCode().value());

    Interpretation::Log::preparation.Log(Logging::Level::TRACE, [&](Stream::Output& out) {
        Descripted desc(out, Descriptor(session, method));
        ResolvingOutput resolving(session, desc);
        resolving << code;
        Disasm(desc, code, &resolver);
    });

    auto resOpt = Rewrite(session, code, resolver, heap, method);
    if (!resOpt.has_value()) {
        return std::nullopt;
    }
    auto res = *resOpt;

    Interpretation::Log::preparation.Log(Logging::Level::TRACE, [&](Stream::Output& out) {
        Descripted desc(out, Descriptor(session, method));

        desc.Print("bytecode: {} {}", Hex(res.code.bytecode), res.code.bytecodeSize);
        desc.NewLine();
        desc.Print("literals: {} {}", Hex(res.code.literals->_table), res.code.literals->_byteSize / 8);
        desc.NewLine();

        desc << res;
        Cbc::RT::Log(res.code, desc);
    });

    return res;
}

} // namespace Cbc
