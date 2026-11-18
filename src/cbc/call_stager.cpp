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

bool CallStager::CheckArgCount(
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
    size_t paramCount  = sig.params.Size() - (sig.isVarArg ? 1 : 0); // excluding vararg label
    expected          += paramCount;
    expected          += sig.funcVarCount;
    expected          += flags.hasOuterTi;

    if (expected != argCount) {
        Stream::StringBuffer buf;
        buf.PrintLn(
            "Expected = {}. Actual = {}. ps = {}, ftvs = {}, sret = {}, outerti = {}, abi_kind = {}",
            expected,
            argCount,
            paramCount,
            sig.funcVarCount,
            flags.isSRet,
            flags.hasOuterTi,
            flags.abiKind
        );
        Fail(result, buf.ToString());
        return false;
    }
    return true;
}

void CallStager::Fail(StagingResult& result, std::string&& msg)
{
    result.ok      = false;
    result.failure = std::move(msg);
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
    ASSERT(!sig.isVarArg);
    int funcVarsCount = sig.funcVarCount;
    if (!CheckArgCount(args.Size(), flags, sig, result)) {
        return result;
    }

    MoveResolver moves;
    TrackedAbiAssigner abi([&](Location target, CbcTypeKind kind) {
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
            ASSERT(derivedLoc.Kind() == Location::IREG);
            result.derived = derivedLoc.IRegIdx();
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

    if (!moves.Resolve(moveEmitter)) {
        Fail(result, "Register kind mismatch");
        return result;
    }

    result.maxParamPassingSize =
        std::max(result.maxParamPassingSize, static_cast<uint32_t>(abi.MaxStackSlot()) * STACK_SLOT_SIZE);
    return result;
}

std::optional<CAbiStructPlan> CallStager::StageForeignStructRet(const CallSignature& sig, const CallAbiFlags& flags)
{
    if (!flags.isSRet) {
        return std::nullopt;
    }

    Utils::Vector<CAbiField> fields0;
    if (!FlattenCAbiFields(*resolver, sig.retType, 0, fields0)) {
        return std::nullopt;
    }

    auto size0 = resolver->fieldManager->GetFlatSize(sig.retType);
    if (!size0.has_value()) {
        return std::nullopt;
    }

    auto size   = size0.value();
    auto fields = Utils::Span<CAbiField>(fields0.Data(), fields0.Size());
    return ClassifyCAbiStructRet(fields, size);
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
    Utils::Vector<CAbiField> fields0;
    if (!FlattenCAbiFields(*resolver, param.term, 0, fields0)) {
        return false;
    }

    UNWRAP_OPT_B(size, resolver->fieldManager->GetFlatSize(param.term));
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
    const CallSignature& sig,
    std::optional<CAbiStructPlan> retPlan
)
{
    ForeignStaging staging;
    auto& result = staging.result;

    ASSERT(sig.funcVarCount == 0);
    ASSERT(flags.abiKind == Image::MethodAbiKind::STATIC);
    ASSERT(!flags.hasOuterTi);

    int varArgLabelsCnt = (sig.isVarArg ? 1 : 0);
    if (!CheckArgCount(args.Size(), flags, sig, result)) {
        return staging;
    }

    MoveResolver moves;
    TrackedAbiAssigner abi([&](Location target, CbcTypeKind kind) {
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
                Fail(result, "failed to stage by-value struct argument");
                return staging;
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
    uint32_t copyOffset = static_cast<uint32_t>(abi.MaxStackSlot()) * STACK_SLOT_SIZE;
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
        Fail(result, "Register kind mismatch");
        return staging;
    }

    for (const auto& copy : structCopies) {
        // Done after moves resolve, so copy.ptrLoc doesn't contain any useful data.
        if (copy.ptrLoc.Kind() == Location::SLOT) {
            moveEmitter.LeaParamArea(IReg::IR_ACC, copy.dstOffset);
            moveEmitter.AssignSlotFromIReg(copy.ptrLoc.SlotIdx(), IReg::IR_ACC);
        } else {
            moveEmitter.LeaParamArea(IReg::From(copy.ptrLoc.IRegIdx()), copy.dstOffset);
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

    staging.stackSlots         = abi.MaxStackSlot();
    staging.fregs              = abi.MaxFRegs();
    result.maxParamPassingSize = std::max(result.maxParamPassingSize, copyOffset);
    return staging;
}

CallSignature MakeCallSignature(const Resolution::MethodSignature& sig)
{
    CallSignature info;
    info.retType      = sig.ResType().term;
    info.isVarArg     = sig.isVarArg;
    info.funcVarCount = sig.tvars.GetLength();

    auto params = sig.Params();
    info.params.Reserve(params.Size());
    for (auto term : params) {
        ParamInfo param;
        param.term = term;
        if (term.GetKind() == Engine::TermKind::VARARGLABEL) {
            param.isVarArgLabel = true;
        } else if (term.GetKind() == Engine::TermKind::VARRAY) {
            param.isVArray = true;
        } else {
            param.kind = ArgTypeTraits<Engine::Term> {}.Kind(term);
        }
        info.params.PushBack(param);
    }
    return info;
}

} // namespace Cbc
