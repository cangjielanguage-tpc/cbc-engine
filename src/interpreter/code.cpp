#include "interpreter/code.h"
#include "cbc/abi.h"
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

    Cbc::TrackedAbiAssigner abi([&](Cbc::Location target, Cbc::ArgKind kind) {
        if (kind == Cbc::ArgKind::FLOAT) {
            totalFloatArgs++;
            return;
        }
        totalIntArgs++;
        if (target.Kind() == Cbc::Location::IREG) {
            int idx = target.IRegIdx();
            if (kind == Cbc::ArgKind::REC)
                stackPtrParams |= (1u << idx);
            if (kind == Cbc::ArgKind::REF)
                referenceParams |= (1u << idx);
        }
    });

    if (flags.isSRet) {
        abi.ConsumeSret();
    }

    if (flags.isMut) {
        auto derived = abi.Consume(Cbc::ArgKind::INT);
        abi.Consume(Cbc::ArgKind::REF);
        derivedPairs |= (1u << derived.IRegIdx());
    } else if (flags.referenceReceiver) {
        abi.Consume(Cbc::ArgKind::REF);
    } else if (flags.recordReceiver) {
        abi.Consume(Cbc::ArgKind::REC);
    }

    int termIdx = 0;
    int length  = signature.GetLength() - 1; // skip ret type term.
    while (termIdx < length) {
        auto term = signature.Subterm(termIdx);
        Cbc::ArgKind kind;
        if (term.IsFloat()) {
            kind = Cbc::ArgKind::FLOAT;
        } else if (term.IsRecord()) {
            kind = Cbc::ArgKind::REC;
        } else if (term.IsReference()) {
            kind = Cbc::ArgKind::REF;
        } else {
            kind = Cbc::ArgKind::INT;
        }
        abi.Consume(kind);
        termIdx++;
    }

    for (int i = 0; i < flags.hasThisTypeInfo + flags.hasOuterTi + flags.funcVars; i++) {
        abi.Consume(Cbc::ArgKind::INT);
    }

    bool hasTailReg = (totalIntArgs > IREG_PARAM_PASSING_AMOUNT);
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
