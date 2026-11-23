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

bool CallStager::CheckManagedArgCount(
    size_t argCount, const CallAbiFlags& flags, const CallSignature& sig, StagingResult& result
)
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
    expected += sig.params.Size();
    expected += sig.funcVarCount;
    expected += flags.hasOuterTi;

    if (expected != argCount) {
        Stream::StringBuffer buf;
        buf.PrintLn(
            "Expected = {}. Actual = {}. ps = {}, ftvs = {}, sret = {}, outerti = {}, abi_kind = {}",
            expected,
            argCount,
            sig.params.Size(),
            sig.funcVarCount,
            flags.isSRet,
            flags.hasOuterTi,
            flags.abiKind
        );
        result = Fail(buf.ToString());
        return false;
    }
    return true;
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

StagingResult CallStager::StageManaged(
    Utils::Span<const Location> args,
    const CallAbiFlags& flags,
    const CallSignature& sig,
    Location* outerTiLoc,
    Location* thisTiLoc,
    bool genericParams
)
{
    StagingResult result;
    int funcVarsCount = sig.funcVarCount;
    if (!CheckManagedArgCount(args.Size(), flags, sig, result)) {
        return result;
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
            ASSERT(derivedLoc.loc.Kind() == Location::IREG);
            result.derived = derivedLoc.loc.IRegIdx();
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

    auto params = sig.params;
    ASSERT(args.Size() >= params.Size() + idx);

    if (genericParams) {
        // Special case for call.closure.g
        // The referenced type (of closure) in instruction doesn't encode parameters as boxes.
        for (size_t j = 0; j < params.Size(); ++idx, ++j) {
            moves.AddMove(abi.Consume(CbcTypeKind::REF), args[idx]);
        }
    } else {
        for (size_t j = 0; j < params.Size(); ++j) {
            moves.AddMove(abi.Consume(params[j].kind), args[idx++]);
        }
    }

    ASSERT(funcVarsCount <= static_cast<int>(args.Size()));
    for (int j = 0; j < funcVarsCount; idx++, j++) {
        moves.AddMove(abi.Consume(CbcTypeKind::I64), args[idx]);
    }

    if (flags.hasOuterTi) {
        auto loc = abi.Consume(CbcTypeKind::I64);
        moves.AddMove(loc, args[idx++]);
        if (outerTiLoc)
            *outerTiLoc = loc.loc;
    }

    if (flags.abiKind == Image::MethodAbiKind::HAS_THIS_TI) {
        auto loc = abi.Consume(CbcTypeKind::I64);
        moves.AddMove(loc, args[idx++]);
        if (thisTiLoc)
            *thisTiLoc = loc.loc;
    }

    if (!moves.Resolve(moveEmitter)) {
        return Fail("Register kind mismatch");
    }

    result.maxParamPassingSize = std::max(result.maxParamPassingSize, static_cast<uint32_t>(abi.MaxStackBytes()));
    return result;
}

std::optional<CAbiStructPlan> CallStager::StageForeignStructRet(
    const ForeignCallSignature& sig, const CallAbiFlags& flags
)
{
    if (!flags.isSRet) {
        return std::nullopt;
    }
    return ClassifyCAbiStructRet(cAbi, sig.retFields, sig.retSize);
}

bool CallStager::StageForeignStructArg(
    Location srcAddr,
    const ParamInfo& param,
    bool varArgPos,
    MoveResolver& moves,
    AbiAssigner& abi,
    Utils::Vector<ForeignStructCopy>& copies
)
{
    auto size = param.flatSize;
    auto plan = ClassifyCAbiStruct(cAbi, param.fields, size, varArgPos, abi.FreeIRegs(), abi.FreeFRegs());
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
            copies.PushBack(ForeignStructCopy { srcAddr, abi.Consume(CbcTypeKind::I64), size });
            break;
        }
    }
    return true;
}

ForeignStaging CallStager::StageForeign(
    Utils::Span<const Location> args,
    const CallAbiFlags& flags,
    const ForeignCallSignature& sig,
    std::optional<CAbiStructPlan> retPlan
)
{
    ForeignStaging staging;
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
            moves.AddMove(abi.Consume(CbcTypeKind::I64), args[idx++]);
        } else if (params[j].kind == CbcTypeKind::REC) {
            // Pass struct argument by value.
            if (!StageForeignStructArg(args[idx++], params[j], isVarArg, moves, abi, structCopies)) {
                return { 0, 0, Fail("failed to stage by-value struct argument") };
            }
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

CallSignature MakeCallSignature(Engine::Term signature)
{
    CallSignature info;
    uint32_t paramCount = signature.GetLength() - 1; // skip ret type
    info.params.Reserve(paramCount);
    for (uint32_t i = 0; i < paramCount; i++) {
        ParamInfo param;
        param.kind = ArgTypeTraits<Engine::Term> {}.Kind(signature.Subterm(i));
        info.params.PushBack(param);
    }
    return info;
}

CallSignature MakeCallSignature(const Resolution::MethodSignature& sig)
{
    ASSERT(!sig.isVarArg);

    CallSignature info = MakeCallSignature(sig.term);
    info.funcVarCount  = sig.tvars.GetLength();
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
        ParamInfo param;
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
    const CallSignature& sig, const CallAbiFlags& flags, const AbiAssigner::PlatformDescription& desc
)
{
    Utils::Vector<TailParamInfo> tail;
    AbiAssigner abi(desc);

    auto record = [&](ArgTarget target, CbcTypeKind kind) {
        if (target.loc.Kind() != Location::SLOT) {
            return;
        }
        tail.PushBack(TailParamInfo {
            .offset = static_cast<uint32_t>(target.loc.SlotIdx()) * STACK_SLOT_SIZE + target.byteOffset,
            .ldk    = TailLoadKind(kind),
        });
    };

    if (flags.isSRet) {
        record(abi.ConsumeSret(), CbcTypeKind::REC);
    }

    // Same consumption order as StageManaged: receiver, parameters, then the
    // hidden trailing arguments.
    switch (flags.abiKind) {
        case Image::MethodAbiKind::MUT: {
            record(abi.Consume(CbcTypeKind::REC), CbcTypeKind::REC);
            record(abi.Consume(CbcTypeKind::REF), CbcTypeKind::REF);
            break;
        }
        case Image::MethodAbiKind::REF_RECEIVER:   record(abi.Consume(CbcTypeKind::REF), CbcTypeKind::REF); break;
        case Image::MethodAbiKind::PRIM_RECEIVER:  record(abi.Consume(CbcTypeKind::I64), CbcTypeKind::I64); break;
        case Image::MethodAbiKind::FPRIM_RECEIVER: record(abi.Consume(CbcTypeKind::F64), CbcTypeKind::F64); break;
        case Image::MethodAbiKind::REC_RECEIVER:   record(abi.Consume(CbcTypeKind::REC), CbcTypeKind::REC); break;
        case Image::MethodAbiKind::STATIC:
        case Image::MethodAbiKind::HAS_THIS_TI:    break;
    }

    for (const auto& param : sig.params) {
        record(abi.Consume(param.kind), param.kind);
    }

    int hiddenCount = sig.funcVarCount + flags.hasOuterTi + (flags.abiKind == Image::MethodAbiKind::HAS_THIS_TI);
    for (int i = 0; i < hiddenCount; i++) {
        record(abi.Consume(CbcTypeKind::I64), CbcTypeKind::I64);
    }

    return tail;
}

} // namespace Cbc
