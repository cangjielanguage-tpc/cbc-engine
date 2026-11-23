#include "interpreter/code.h"
#include "cbc/abi.h"
#include "cbc/call_stager.h"
#include "engine/image/flags.h"
#include "interpreter/ectype.h"
#include "platform_traits.h"
#include "utils/assertion.h"
#include "utils/ostream.h"
#include <cstdint>
#include <string_view>

namespace Interpretation {

Stream::Output& operator<<(Stream::Output& out, const ExecBytecodeInfo& bc)
{
    using namespace Stream;
    Stream::Indented out2(out, 2);
    Stream::Indented out4(out, 4);

    out << "ExecBytecodeInfo {" << endl;
    out2 << "untypedSlotsCount: " << bc.untypedSlotCount << endl
         << "typedSlotsCount: " << bc.gcInfo.refOffsets.Size() << endl
         << "frameSize: " << bc.frameSize << endl;

    out2 << "GCMap {" << endl;
    for (const auto& entry : bc.gcInfo.positionalInfo) {
        out4 << "rtPos: " << entry.rewrittenPos << ", regMask: " << entry.regMask << ", ";
        Std::Vector::Print(out4, entry.untypedRefSlots);
        Std::Vector::Print(out4, entry.paramRefSlots);
        Std::Vector::Print(out4, entry.paramRecSlots);
        Std::Vector::Print(out4, entry.mutPairs, [](Stream::Output& out, std::pair<Resource, Resource> p) {
            out.Print("({}, {})", p.first.idx, p.second.idx);
        });
        out4 << endl;
    }
    out2 << "}" << endl;

    return out << "}" << endl;
}

AbiInfo BuildAbiInfo(Engine::Session& session, Engine::Term signature, AbiInfoFlags flags)
{
    uint16_t derivedPairs    = 0;
    uint16_t stackPtrParams  = 0;
    uint16_t referenceParams = 0;
    int totalIntArgs         = 0;
    int totalFloatArgs       = 0;

    Cbc::TrackedAbiAssigner abi([&](Cbc::Location target, Cbc::CbcTypeKind kind) {
        if (Cbc::IsFloat(kind)) {
            totalFloatArgs++;
            return;
        }
        totalIntArgs++;
        if (target.Kind() == Cbc::Location::IREG) {
            int idx = target.IRegIdx();
            if (kind == Cbc::CbcTypeKind::REC)
                stackPtrParams |= (1u << idx);
            if (kind == Cbc::CbcTypeKind::REF)
                referenceParams |= (1u << idx);
        }
    });

    auto callSig         = Cbc::MakeManagedCallSignature(signature);
    callSig.funcVarCount = flags.funcVars;
    for (auto arg : Cbc::ManagedArgSequence(callSig, { flags.isSRet, flags.hasOuterTi, flags.abiKind }, false)) {
        auto target = arg.role == Cbc::ManagedArg::Role::Sret ? abi.ConsumeSret() : abi.Consume(arg.kind);
        if (arg.role == Cbc::ManagedArg::Role::Derived) {
            ASSERT(target.loc.Kind() == Cbc::Location::IREG);
            derivedPairs |= (1u << target.loc.IRegIdx());
        }
    }

    bool hasTailReg = (totalIntArgs > IREG_PARAM_PASSING_AMOUNT || totalFloatArgs > FREG_ABI_AMOUNT);
    if (hasTailReg) {
        stackPtrParams |= (1u << IReg::TAIL_REG);
    }

    int iregParamCount = totalIntArgs;
    if (iregParamCount > IREG_PARAM_PASSING_AMOUNT)
        iregParamCount = IREG_PARAM_PASSING_AMOUNT;
    int fregParamCount = totalFloatArgs;
    if (fregParamCount > FREG_ABI_AMOUNT)
        fregParamCount = FREG_ABI_AMOUNT;

    return {
        .stackPtrParams  = stackPtrParams,
        .referenceParams = referenceParams,
        .derivedPairs    = derivedPairs,
        .iregParamCount  = (uint8_t)iregParamCount,
        .fregParamCount  = (uint8_t)fregParamCount,
        .isSRet          = flags.isSRet,
        .hasTailReg      = hasTailReg,
    };
}

} // namespace Interpretation
