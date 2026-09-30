#include "interpreter/code.h"
#include "interpreter/ectype.h"
#include "platform_traits.h"
#include "utils/assertion.h"
#include <cstdint>

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
        Std::Vector::Print(out4, entry.untypedRefSlotsInfo);
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

    int fargIdx      = 0; // FR0
    int iargIdx      = 1; // IR1
    unsigned slotIdx = 0;

    auto nextArg = [&](bool isFloat) -> uint16_t {
        if (isFloat && fargIdx < FREG_ABI_AMOUNT) {
            ++fargIdx;
            return StaticCallTypeInfoArgs::NONE;
        }
        if (!isFloat && iargIdx <= IREG_PARAM_PASSING_AMOUNT) {
            return iargIdx++;
        }
        auto location = Cbc::IReg::VIRT_COUNT + slotIdx++;
        ASSERT(location <= UINT16_MAX);
        return static_cast<uint16_t>(location);
    };

    if (HAS_SRET_SHIFT && flags.isSRet) {
        // stack-return position on this platform is param passing register.
        stackPtrParams |= (1 << nextArg(false));
    } else if (!HAS_SRET_SHIFT && flags.isSRet) {
        // stack-return position on this platform is using special register.
        stackPtrParams |= (1 << IReg::SRET_IR);
    }

    if (flags.isMut) {
        auto derived     = nextArg(false);
        auto base        = nextArg(false);
        derivedPairs    |= (1 << derived);
        referenceParams |= (1 << base);
    } else if (flags.referenceReceiver) {
        referenceParams |= (1 << nextArg(false));
    } else if (flags.recordReceiver) {
        stackPtrParams |= (1 << nextArg(false));
    }

    ASSERT(iargIdx < IREG_PARAM_PASSING_AMOUNT);

    int termIdx = 0;
    int length  = signature.GetLength() - 1; // skip ret type term.
    while (termIdx < length) {
        auto term     = signature.Subterm(termIdx);
        int isRecord  = term.IsRecord();
        int isRef     = term.IsReference();
        auto location = nextArg(term.IsFloat());
        int isReg     = location < Cbc::IReg::VIRT_COUNT;

        if (isReg) {
            stackPtrParams  |= (isRecord << location);
            referenceParams |= (isRef << location);
        }
        termIdx++;
    }

    for (int i = 0; i < flags.funcVars; ++i) {
        nextArg(false);
    }
    auto outerTi = flags.hasOuterTi ? nextArg(false) : StaticCallTypeInfoArgs::NONE;
    auto thisTi  = flags.hasThisTypeInfo ? nextArg(false) : StaticCallTypeInfoArgs::NONE;

    bool hasTailReg = slotIdx > 0;

    if (hasTailReg) {
        // Tail register holds a pointer to position, where stack-passed parameters are located.
        stackPtrParams |= (1 << IReg::TAIL_REG);
    }

    // Adjust number of ireg/freg params.
    if (iargIdx > IREG_PARAM_PASSING_AMOUNT) // do not account for special register for SRET (if it is exist)
        iargIdx = IREG_PARAM_PASSING_AMOUNT;
    if (fargIdx > FREG_ABI_AMOUNT)
        fargIdx = FREG_ABI_AMOUNT;

    return {
        .stackPtrParams         = stackPtrParams,
        .referenceParams        = referenceParams,
        .derivedPairs           = derivedPairs,
        .staticCallTypeInfoArgs = { outerTi, thisTi },
        .iregParamCount         = (uint8_t)iargIdx,
        .fregParamCount         = (uint8_t)fargIdx,
        .isSRet                 = flags.isSRet,
        .hasTailReg             = hasTailReg,
    };
}

} // namespace Interpretation
