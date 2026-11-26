#include "interpreter/code.h"
#include "cbc/abi.h"
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
    out2 << "version: " << bc.version << endl
         << "ownerFuh: " << Hex(bc.ownerFuh) << endl
         << "bytecodeSize: " << bc.code.bytecodeSize << endl
         << "literalCount: " << bc.code.literals->size() << endl
         << "frameSize: " << bc.frameSize << endl
         << "staticFrameSize: " << bc.staticFrameSize << endl
         << "untypedSlotsCount: " << bc.untypedSlotCount << endl
         << "savedIRegs: " << Hex(bc.savedIRegs.value) << endl
         << "savedFRegs: " << Hex(bc.savedFRegs.value) << endl
         << "savedIRegsMask: " << Hex(bc.savedIRegs.Mask()) << endl
         << "savedFRegsMask: " << Hex(bc.savedFRegs.Mask()) << endl;

    const auto& abi = bc.abiInfo;
    out2 << "AbiInfo {" << endl;
    out4 << "stackPtrParams: " << Hex(abi.stackPtrParams) << endl
         << "referenceParams: " << Hex(abi.referenceParams) << endl
         << "derivedPairs: " << Hex(abi.derivedPairs) << endl
         << "outerTi: " << abi.staticCallTypeInfoArgs.outerTi << endl
         << "thisTi: " << abi.staticCallTypeInfoArgs.thisTi << endl
         << "iregParamCount: " << abi.iregParamCount << endl
         << "fregParamCount: " << abi.fregParamCount << endl
         << "isSRet: " << abi.isSRet << endl
         << "hasTailReg: " << abi.hasTailReg << endl;
    out2 << "}" << endl;

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

    out2 << "StackPtrsMap {" << endl;
    for (const auto& entry : bc.stackPtrsInfo.positionalInfo) {
        out4 << "rtPos: " << entry.rewrittenPos << ", resources: ";
        Std::Vector::Print(out4, entry.resources, [](Stream::Output& out, const Resource& resource) {
            out << resource.idx;
        });
        out4 << endl;
    }
    out2 << "}" << endl;

    out2 << "ExceptionPositions: [";
    const char* separator = "";
    for (const auto& [rtPos, cbcPos] : bc.bcPositionsForExceptions) {
        out2 << separator << cbcPos << "->" << rtPos;
        separator = ", ";
    }
    out2 << "]" << endl;

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

    if (flags.isSRet) {
        abi.ConsumeSret();
    }

    switch (flags.abiKind) {
        case Image::MethodAbiKind::MUT: {
            auto derived = abi.Consume(Cbc::CbcTypeKind::I64);
            abi.Consume(Cbc::CbcTypeKind::REF);
            derivedPairs |= (1u << derived.IRegIdx());
            break;
        }
        case Image::MethodAbiKind::REF_RECEIVER:   abi.Consume(Cbc::CbcTypeKind::REF); break;
        case Image::MethodAbiKind::REC_RECEIVER:   abi.Consume(Cbc::CbcTypeKind::REC); break;
        case Image::MethodAbiKind::PRIM_RECEIVER:  abi.Consume(Cbc::CbcTypeKind::I64); break;
        case Image::MethodAbiKind::FPRIM_RECEIVER: abi.Consume(Cbc::CbcTypeKind::F64); break;
        case Image::MethodAbiKind::STATIC:
        case Image::MethodAbiKind::HAS_THIS_TI:    ;
    }

    int termIdx = 0;
    int length  = signature.GetLength() - 1; // skip ret type term.
    while (termIdx < length) {
        abi.Consume(signature.Subterm(termIdx++));
    }

    bool hasThisTi = (flags.abiKind == Image::MethodAbiKind::HAS_THIS_TI);
    for (int i = 0; i < hasThisTi + flags.hasOuterTi + flags.funcVars; i++) {
        abi.Consume(Cbc::CbcTypeKind::I64);
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
