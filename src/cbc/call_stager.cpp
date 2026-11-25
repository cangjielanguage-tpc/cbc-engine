#include "call_stager.h"
#include "cbc/c_abi.h"
#include "engine/image/flags.h"
#include "resolution/resolution.h"
#include "runtimesupport/runtime.h"
#include "utils/assertion.h"
#include "utils/math.h"

#include <tuple>
#include <utility>

namespace Cbc {

using LDK = Format::LoadAccessKind;

StagingResult CallStager::Fail(std::string&& msg) { return { .ok = false, .failure = std::move(msg) }; }

Utils::Vector<ManagedArg> ManagedArgSequence(
    const ManagedCallSignature& sig, const CallAbiFlags& flags, bool genericParams
)
{
    Utils::Vector<ManagedArg> seq;
    seq.Reserve(sig.params.Size() + 5);
    if (flags.isSRet) {
        seq.PushBack({ CbcTypeKind::REC, ManagedArg::Role::Sret });
    }
    switch (flags.abiKind) {
        case Image::MethodAbiKind::MUT:
            seq.PushBack({ CbcTypeKind::I64, ManagedArg::Role::Derived });
            seq.PushBack({ CbcTypeKind::REF, ManagedArg::Role::Receiver });
            break;
        case Image::MethodAbiKind::REF_RECEIVER:  seq.PushBack({ CbcTypeKind::REF, ManagedArg::Role::Receiver }); break;
        case Image::MethodAbiKind::PRIM_RECEIVER: seq.PushBack({ CbcTypeKind::I64, ManagedArg::Role::Receiver }); break;
        case Image::MethodAbiKind::FPRIM_RECEIVER:
            seq.PushBack({ CbcTypeKind::F64, ManagedArg::Role::Receiver });
            break;
        case Image::MethodAbiKind::REC_RECEIVER: seq.PushBack({ CbcTypeKind::REC, ManagedArg::Role::Receiver }); break;
        case Image::MethodAbiKind::STATIC:
        case Image::MethodAbiKind::HAS_THIS_TI:  break;
    }
    for (auto param : sig.params) {
        // call.closure.g passes every parameter boxed, whatever the signature says.
        seq.PushBack({ genericParams ? CbcTypeKind::REF : param, ManagedArg::Role::Param });
    }
    for (int i = 0; i < sig.funcVarCount; i++) {
        seq.PushBack({ CbcTypeKind::I64, ManagedArg::Role::FuncVar });
    }
    if (flags.hasOuterTi) {
        seq.PushBack({ CbcTypeKind::I64, ManagedArg::Role::OuterTi });
    }
    if (flags.abiKind == Image::MethodAbiKind::HAS_THIS_TI) {
        seq.PushBack({ CbcTypeKind::I64, ManagedArg::Role::ThisTi });
    }
    return seq;
}

StagingResult CallStager::StageManaged(
    Utils::Span<const Location> args,
    const CallAbiFlags& flags,
    const ManagedCallSignature& sig,
    Location* outerTiLoc,
    Location* thisTiLoc,
    bool genericParams
)
{
    StagingResult result;
    auto seq = ManagedArgSequence(sig, flags, genericParams);
    if (seq.Size() != args.Size()) {
        Stream::StringBuffer buf;
        buf.PrintLn(
            "Expected = {}. Actual = {}. ps = {}, ftvs = {}, sret = {}, outerti = {}, abi_kind = {}",
            seq.Size(),
            args.Size(),
            sig.params.Size(),
            sig.funcVarCount,
            flags.isSRet,
            flags.hasOuterTi,
            flags.abiKind
        );
        return Fail(buf.ToString());
    }

    MoveResolver moves;
    TrackedAbiAssigner abi(desc, [&](Location target, CbcTypeKind kind) {
        if (target.Kind() == Location::SLOT) {
            if (kind == CbcTypeKind::REF)
                result.paramRefSlots.PushBack(target.SlotIdx());
            if (kind == CbcTypeKind::REC)
                result.paramRecSlots.PushBack(target.SlotIdx());
        }
    });

    int idx = 0;
    for (auto arg : seq) {
        auto target = arg.role == ManagedArg::Role::Sret ? abi.ConsumeSret() : abi.Consume(arg.kind);
        moves.AddMove(target, args[idx++]);
        switch (arg.role) {
            case ManagedArg::Role::Derived:
                ASSERT(target.loc.Kind() == Location::IREG);
                result.derived = target.loc.IRegIdx();
                break;
            case ManagedArg::Role::OuterTi:
                if (outerTiLoc)
                    *outerTiLoc = target.loc;
                break;
            case ManagedArg::Role::ThisTi:
                if (thisTiLoc)
                    *thisTiLoc = target.loc;
                break;
            default: break;
        }
    }

    if (!moves.Resolve(moveEmitter)) {
        return Fail("Register kind mismatch");
    }

    result.maxParamPassingSize = std::max(result.maxParamPassingSize, static_cast<uint32_t>(abi.MaxStackBytes()));
    return result;
}

bool CallStager::CheckForeignArgCount(
    size_t argCount, const CallAbiFlags& flags, const ForeignCallSignature& sig, StagingResult& result
)
{
    ASSERT(flags.abiKind == Image::MethodAbiKind::STATIC);
    ASSERT(!flags.hasOuterTi);

    size_t expected = flags.isSRet;
    // The vararg label is part of the reference signature but is not passed.
    expected += sig.params.Size() - (sig.isVarArg ? 1 : 0);

    if (expected != argCount) {
        Stream::StringBuffer buf;
        buf.PrintLn(
            "Expected = {}. Actual = {}. ps = {}, sret = {}, abi_kind = {}",
            expected,
            argCount,
            sig.params.Size(),
            flags.isSRet,
            flags.abiKind
        );
        result = Fail(buf.ToString());
        return false;
    }
    return true;
}

std::optional<CAbiStructPlan> CallStager::StageForeignStructRet(
    const ForeignCallSignature& sig, const CallAbiFlags& flags
)
{
    if (!flags.isSRet) {
        return std::nullopt;
    }
    return ClassifyCAbiStructRet(desc.cAbi, sig.retFields, sig.retSize);
}

void CallStager::StageForeignStructArg(
    Location srcAddr,
    const ForeignParam& param,
    bool varArgPos,
    MoveResolver& moves,
    AbiAssigner& abi,
    Utils::Vector<ForeignStructCopy>& copies
)
{
    auto size = param.flatSize;
    auto plan = ClassifyCAbiStruct(
        desc.cAbi, param.fields, size, varArgPos, desc.onlyOnStackVarargs, abi.FreeIRegs(), abi.FreeFRegs()
    );
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
            for (uint32_t offset = 0; offset < size; offset += STACK_SLOT_SIZE) {
                moves.AddLoadFromAddr(Location::Slot(base + offset / STACK_SLOT_SIZE), srcAddr, offset, LDK::LD_64);
            }
            break;
        }
        case CAbiPassBy::REF: {
            // Stage placing struct after all args and consume location for reference.
            auto target = varArgPos ? abi.ConsumeVarArg(CbcTypeKind::I64) : abi.Consume(CbcTypeKind::I64);
            copies.PushBack(ForeignStructCopy { srcAddr, target, size });
            break;
        }
    }
}

ForeignStagingResult CallStager::StageForeign(
    Utils::Span<const Location> args,
    const CallAbiFlags& flags,
    const ForeignCallSignature& sig,
    std::optional<CAbiStructPlan> retPlan
)
{
    ForeignStagingResult staging;
    auto& result = staging.result;

    int varArgLabelsCnt = (sig.isVarArg ? 1 : 0);
    if (!CheckForeignArgCount(args.Size(), flags, sig, result)) {
        return staging;
    }

    MoveResolver moves;
    TrackedAbiAssigner abi(desc, [&](Location target, CbcTypeKind kind) {
        // Foreign functions cannot receive managed references.
        ASSERT(kind != CbcTypeKind::REF);
        if (target.Kind() == Location::SLOT && kind == CbcTypeKind::REC)
            result.paramRecSlots.PushBack(target.SlotIdx());
    });
    Utils::Vector<ForeignStructCopy> structCopies;

    int idx = 0;

    if (retPlan.has_value()) {
        idx++;
        if (retPlan->passBy != CAbiPassBy::EMPTY && retPlan->passBy != CAbiPassBy::REGS) {
            moves.AddMove(abi.ConsumeSret(), args[0]);
        }
    }

    auto params = sig.params;
    ASSERT(args.Size() >= params.Size() + idx - varArgLabelsCnt);

    bool isVarArg = false;
    for (size_t j = 0; j < params.Size(); ++j) {
        if (params[j].isVarArgLabel) {
            isVarArg = true;
            continue;
        }

        if (params[j].isVArray) {
            // VArrays are always passed by reference.
            auto target = isVarArg ? abi.ConsumeVarArg(CbcTypeKind::I64) : abi.Consume(CbcTypeKind::I64);
            moves.AddMove(target, args[idx++]);
        } else if (params[j].kind == CbcTypeKind::REC) {
            // Pass struct argument by value.
            StageForeignStructArg(args[idx++], params[j], isVarArg, moves, abi, structCopies);
        } else if (!isVarArg) {
            moves.AddMove(abi.Consume(params[j].kind), args[idx++]);
        } else {
            moves.AddMove(abi.ConsumeVarArg(params[j].kind), args[idx++]);
        }
    }

    int sretSpillSlot = -1;
    if (retPlan.has_value() && retPlan->PiecesNum() > 0) {
        // If sretAddrLocation is reg, move resolver can spoil it, so save it to stack before resolution.
        auto sretAddrLocation = args[0];
        if (sretAddrLocation.Kind() != Location::SLOT) {
            sretSpillSlot = abi.ConsumeStackArea(STACK_SLOT_SIZE);
            moves.AddMove(Location::Slot(sretSpillSlot), sretAddrLocation);
        }
    }

    // By-reference struct copies are placed after all stack arguments:
    // the I2N adapter copies only the stack-argument area, so each copy
    // stays in the frame and only its pointer is passed.
    uint32_t copyOffset = static_cast<uint32_t>(abi.MaxStackBytes());
    for (auto& copy : structCopies) {
        copy.dstOffset = copyOffset;
        for (uint32_t offset = 0; offset < copy.size; offset += STACK_SLOT_SIZE) {
            moves.AddLoadFromAddr(
                Location::Slot((copyOffset + offset) / STACK_SLOT_SIZE), copy.srcAddr, offset, LDK::LD_64
            );
        }
        copyOffset += MathUtils::AlignUp(copy.size, STACK_SLOT_SIZE);
    }

    if (!moves.Resolve(moveEmitter)) {
        return { 0, 0, Fail("Register kind mismatch") };
    }

    for (const auto& copy : structCopies) {
        // Done after moves resolve, so copy.ptrLoc doesn't contain any useful data.
        if (copy.ptrLoc.loc.Kind() == Location::SLOT) {
            moveEmitter.LeaParamArea(IReg::IR_ACC, copy.dstOffset);
            moveEmitter.AssignSlotFromIReg(
                copy.ptrLoc.loc.SlotIdx(), IReg::IR_ACC, copy.ptrLoc.stk, copy.ptrLoc.byteOffset
            );
        } else {
            moveEmitter.LeaParamArea(IReg::From(copy.ptrLoc.loc.IRegIdx()), copy.dstOffset);
        }
    }

    if (retPlan.has_value() && retPlan->PiecesNum() > 0) {
        // Move sret destination address into ACC. It will be used after call by `EmitFillSRetAreaByRegs`.
        auto sretAddrLocation = args[0];
        if (sretAddrLocation.Kind() != Location::SLOT) {
            moveEmitter.LeaParamArea(IReg::IR_ACC, static_cast<uint32_t>(sretSpillSlot) * STACK_SLOT_SIZE);
            moveEmitter.AssignIRegFromAddr(IReg::IR_ACC, Location::IReg(IReg::IR_ACC), 0, LDK::LD_64);
        } else {
            moveEmitter.AssignIRegFromSlot(IReg::IR_ACC, sretAddrLocation.SlotIdx());
        }
    }

    staging.stackBytes         = abi.MaxStackBytes();
    staging.fregs              = abi.MaxFRegs();
    result.maxParamPassingSize = std::max(result.maxParamPassingSize, copyOffset);
    return staging;
}

ManagedCallSignature MakeManagedCallSignature(Engine::Term signature)
{
    ManagedCallSignature info;
    uint32_t paramCount = signature.GetLength() - 1; // skip ret type
    info.params.Reserve(paramCount);
    for (uint32_t i = 0; i < paramCount; i++) {
        info.params.PushBack(ArgTypeTraits<Engine::Term> {}.Kind(signature.Subterm(i)));
    }
    return info;
}

ManagedCallSignature MakeManagedCallSignature(const Resolution::MethodSignature& sig)
{
    ASSERT(!sig.isVarArg);

    ManagedCallSignature info = MakeManagedCallSignature(sig.term);
    info.funcVarCount         = sig.tvars.GetLength();
    return info;
}

std::optional<ForeignCallSignature> MakeForeignCallSignature(
    Resolution::Resolver& resolver, const Resolution::MethodSignature& sig
)
{
    ForeignCallSignature info;
    info.isVarArg = sig.isVarArg;

    auto params = sig.Params();
    info.params.Reserve(params.Size());
    for (auto term : params) {
        ForeignParam param;
        if (term.GetKind() == Engine::TermKind::VARARGLABEL) {
            param.isVarArgLabel = true;
        } else if (term.GetKind() == Engine::TermKind::VARRAY) {
            param.isVArray = true;
        } else if (term.GetKind() == Engine::TermKind::UNIT) {
            param.kind = CbcTypeKind::I64;
        } else {
            param.kind = ArgTypeTraits<Engine::Term> {}.Kind(term);
            if (param.kind == CbcTypeKind::REC) {
                if (!FlattenCAbiFields(resolver, term, 0, param.fields)) {
                    return std::nullopt;
                }
                auto size = resolver.fieldManager->GetFlatSize(term);
                if (!size.has_value()) {
                    return std::nullopt;
                }
                param.flatSize = *size;
            }
        }
        info.params.PushBack(param);
    }

    auto retType = sig.ResType().term;
    if (retType.IsRecord() && retType.GetKind() != Engine::TermKind::UNIT) {
        if (!FlattenCAbiFields(resolver, retType, 0, info.retFields)) {
            return std::nullopt;
        }
        auto size = resolver.fieldManager->GetFlatSize(retType);
        if (!size.has_value()) {
            return std::nullopt;
        }
        info.retSize = *size;
    }
    return info;
}

/// Load kind of one tail parameter. Records cross the boundary as pointers to
/// the caller-owned copy, so they load as raw words.
static Format::LoadAccessKind TailLoadKind(CbcTypeKind kind)
{
    if (kind == CbcTypeKind::REC) {
        return Format::LoadAccessKind::LD_64;
    }
    return Ldk(kind);
}

Utils::Vector<TailParamInfo> ComputeTailParams(
    const ManagedCallSignature& sig, const CallAbiFlags& flags, const AbiAssigner::PlatformDescription& desc
)
{
    Utils::Vector<TailParamInfo> tail;
    AbiAssigner abi(desc);

    for (auto arg : ManagedArgSequence(sig, flags, false)) {
        auto target = arg.role == ManagedArg::Role::Sret ? abi.ConsumeSret() : abi.Consume(arg.kind);
        if (target.loc.Kind() != Location::SLOT) {
            continue;
        }
        tail.PushBack(TailParamInfo {
            .offset = static_cast<uint32_t>(target.loc.SlotIdx()) * STACK_SLOT_SIZE + target.byteOffset,
            .ldk    = TailLoadKind(arg.kind),
        });
    }
    return tail;
}

} // namespace Cbc
