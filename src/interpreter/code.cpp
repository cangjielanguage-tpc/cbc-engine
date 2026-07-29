#include "code.h"
#include "interpreter/ectype.h"
#include "platform_traits.h"
#include <cstdint>

namespace Interpretation {

Stream::Output& operator<<(Stream::Output& out, const ExecBytecodeInfo& bc)
{
    using namespace Stream;
    Stream::Indented out2(out, 2);
    Stream::Indented out4(out, 4);

    out << "ExecBytecodeInfo {" << endl;
    out2 << "untypedSlotsCount: " << bc.untypedSlotCount << endl
         << "typedSlotsCount: " << bc.gcInfo.typedSlotsInfo.size() << endl
         << "frameSize: " << bc.frameSize << endl;

    out2 << "GCMap {" << endl;
    for (const auto& entry : bc.gcInfo.positionalInfo) {
        out4 << "rtPos: " << entry.rewrittenPos << ", regMask: " << entry.regMask << ", ";
        Std::Vector::Print(out4, entry.untypedRefSlotsInfo);
        out4 << endl;
    }
    out2 << "}" << endl;

    return out << "}" << endl;
}

AbiInfo BuildAbiInfo(Engine::Session& session, Engine::Term signature, AbiInfoFlags flags)
{
    uint16_t derivedPairs     = 0;
    uint16_t adjustableParams = 0;

    int fargIdx = 0; // FR0
    int iargIdx = 1; // IR1

    if (HAS_SRET_SHIFT && flags.isSRet) {
        // stack-return position on this platform is param passing register.
        adjustableParams |= (1 << iargIdx);
        iargIdx++;
    } else if (!HAS_SRET_SHIFT && flags.isSRet) {
        // stack-return position on this platform is using special register.
        adjustableParams |= (1 << IReg::SRET_IR);
    }

    if (flags.isMut) {
        derivedPairs |= (1 << iargIdx);
        iargIdx      += 2; // also skip base ptr
    }

    int termIdx = 0;
    int length  = signature.GetLength() - 1; // skip ret type term.
    while (termIdx < length) {
        auto term    = signature.Subterm(termIdx);
        int isFloat  = term.IsFloat();
        int isRecord = term.IsRecord();
        int isReg    = (iargIdx < IREG_PARAM_PASSING_AMOUNT);

        adjustableParams |= ((isReg && isRecord) << iargIdx);

        // Counters could overflow param passing reg amount.
        fargIdx          += isFloat;
        iargIdx          += !isFloat;
        termIdx++;
    }

    iargIdx += flags.hasThisTypeInfo;
    iargIdx += flags.hasOuterTi;
    iargIdx += flags.funcVars;

    bool hasTailReg = (iargIdx >= IREG_PARAM_PASSING_AMOUNT);

    if (hasTailReg) {
        // Tail register holds a pointer to position, where stack-passed parameters are located.
        adjustableParams |= (1 << IReg::TAIL_REG);
    }

    // Adjust number of ireg/freg params.
    if (iargIdx > IREG_PARAM_PASSING_AMOUNT) // do not account for special register for SRET (if it is exist)
        iargIdx = IREG_PARAM_PASSING_AMOUNT;
    if (fargIdx > FREG_ABI_AMOUNT)
        fargIdx = FREG_ABI_AMOUNT;

    return {
        .adjustableParams = adjustableParams,
        .derivedPairs     = derivedPairs,
        .iregParamCount   = (uint8_t)iargIdx,
        .fregParamCount   = (uint8_t)fargIdx,
        .isSRet           = flags.isSRet,
        .hasTailReg       = hasTailReg,
    };
}

} // namespace Interpretation
