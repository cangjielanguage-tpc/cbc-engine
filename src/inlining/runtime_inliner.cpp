#include "runtime_inliner.h"

#include "cbc/emitter/emitter.h"
#include "cbc/frame.h"
#include "cbc/offsets_index.h"
#include "inlining/inline_plan.h"
#include "inlining/runtime_bytecode.h"
#include "inlining/runtime_frame.h"
#include "interpreter/function_handle.h"
#include "interpreter/literals.h"
#include "utils/assertion.h"
#include "utils/heap.h"

#include <algorithm>
#include <bitset>
#include <cstring>
#include <limits>
#include <optional>
#include <unordered_map>
#include <vector>

namespace Cbc {
namespace {

using Interpretation::ExecBytecodeInfo;

struct PreparedInlineSite {
    uint32_t oldBegin;
    uint32_t oldEnd;
    Utils::Vector<uint8_t> body;
    Utils::Vector<uint8_t> entryCode;
    Utils::Vector<uint8_t> restoreCode;
};

struct PreparedInlining {
    const ExecBytecodeInfo* source;
    ExpandedFrameLayout frameLayout;
    Interpretation::NonVolatileRegs savedIRegs;
    Interpretation::NonVolatileRegs savedFRegs;
    Utils::Vector<uint8_t> bytecode;
    std::vector<RewrittenCodeEdit> edits;
    std::vector<Interpretation::Literal> literals;
};

struct RelocatedMetadata {
    Interpretation::GcInfo gcInfo;
    Interpretation::StackPtrsInfo stackPtrsInfo;
    std::vector<std::pair<uint32_t, uint32_t>> bcPositionsForExceptions;
    InstructionOffsetsIndex offsetsIndex;
};

struct InlineFrameCode {
    Utils::Vector<uint8_t> entry;
    Utils::Vector<uint8_t> restore;
};

InlineFrameCode MakeInlineFrameCode(
    uint32_t callerLocalSize, const ExecBytecodeInfo& callee, uint16_t callerIMask, uint16_t callerFMask
)
{
    Emitter::Emitter entry;
    Emitter::Emitter restore;
    auto offset = -static_cast<int64_t>(callerLocalSize);

    // Use the ordinary callee's I/F save order in the area below the caller's static frame.
    for (auto regs = callee.savedIRegs; !regs.IsEmpty();) {
        const auto index = regs.ExtractReg();
        offset -= STACK_SLOT_SIZE;
        if ((callerIMask & (1U << index)) != 0) {
            const auto reg = IReg::From(index);
            entry.StoreFrame(Format::StoreAccessKind::ST_64, reg, static_cast<int32_t>(offset));
            restore.LoadFrame(Format::LoadAccessKind::LD_64, reg, static_cast<int32_t>(offset));
        }
    }
    for (auto regs = callee.savedFRegs; !regs.IsEmpty();) {
        const auto index = regs.ExtractReg();
        offset -= STACK_SLOT_SIZE;
        if ((callerFMask & (1U << index)) != 0) {
            const auto reg = FReg::From(index);
            entry.StoreFrame(Format::StoreAccessKind::ST_F64, reg, static_cast<int32_t>(offset));
            restore.LoadFrame(Format::LoadAccessKind::LD_F64, reg, static_cast<int32_t>(offset));
        }
    }

    const auto bottom = -static_cast<int64_t>(callerLocalSize) - callee.staticFrameSize;
    if (bottom < offset) {
        entry.ZeroFrame(static_cast<int32_t>(bottom), static_cast<uint32_t>(offset - bottom));
    }
    return { entry.FinishWithoutFixups(), restore.FinishWithoutFixups() };
}

std::optional<PreparedInlineSite> PrepareSite(
    const InlineCallSite& site,
    const ExecBytecodeInfo& caller,
    const std::vector<RuntimeInstruction>& callerInstructions,
    uint32_t expandedCallerStaticSize,
    uint32_t literalBase,
    std::vector<Interpretation::Literal>& literals
)
{
    const auto instruction = std::lower_bound(
        callerInstructions.begin(),
        callerInstructions.end(),
        site.runtimeCallSiteOffset,
        [](const auto& candidate, uint32_t offset) { return candidate.offset < offset; }
    );
    if (instruction == callerInstructions.end() || instruction->offset != site.runtimeCallSiteOffset ||
        instruction->opcode != RT::Opcode::DIRECT_CALL_2I) {
        return std::nullopt;
    }
    ASSERT(instruction->size == RT::B3xi12::SIZE);
    const auto cbcOffset = caller.offsetsIndex.FindContainingCbcOffset(site.runtimeCallSiteOffset);
    if (!cbcOffset.has_value() || cbcOffset.value() != site.cbcCallSiteOffset || site.calleeFuh == nullptr) {
        return std::nullopt;
    }

    auto* const callerEnd = caller.code.bytecode + caller.code.bytecodeSize;
    auto* const callStart = caller.code.bytecode + site.runtimeCallSiteOffset;
    Decoder::ByteReader reader(callStart, caller.code.bytecode, callerEnd);
    const auto call = RT::B3xi12::Decode(reader);
    if (call.xi12.imm12 >= caller.code.literals->size()) {
        return std::nullopt;
    }
    const auto literal = caller.code.literals->at(call.xi12.imm12).uintptr;
    if (literal != reinterpret_cast<uintptr_t>(site.calleeFuh)) {
        return std::nullopt;
    }

    auto* callee        = site.expectedCalleeBytecode;
    auto* currentCallee = site.calleeFuh->bytecode.load();
    if (currentCallee != callee || callee->version != site.expectedCalleeVersion ||
        callee->ownerFuh != site.calleeFuh) {
        return std::nullopt;
    }
    if (!HasSupportedInlineFrame(*callee) || !callee->gcInfo.positionalInfo.Empty() ||
        !callee->stackPtrsInfo.positionalInfo.Empty()) {
        return std::nullopt;
    }
    const auto calleeInstructions = DecodeRuntimeBytecode(callee->code);
    if (!IsSupportedBody(callee->code, calleeInstructions)) {
        return std::nullopt;
    }
    auto body = RelocateInlineBody(
        callee->code, calleeInstructions, expandedCallerStaticSize, literalBase, literals
    );
    if (!body.has_value()) {
        return std::nullopt;
    }

    auto frameCode = MakeInlineFrameCode(
        expandedCallerStaticSize, *callee, caller.savedIRegs.Mask(), caller.savedFRegs.Mask()
    );
    return PreparedInlineSite {
        .oldBegin       = site.runtimeCallSiteOffset,
        .oldEnd         = site.runtimeCallSiteOffset + instruction->size,
        .body           = std::move(*body),
        .entryCode      = std::move(frameCode.entry),
        .restoreCode    = std::move(frameCode.restore),
    };
}

void AppendLiteralTable(std::vector<Interpretation::Literal>& literals, const Interpretation::LiteralTable& table)
{
    if (table.size() != 0) {
        const auto base = literals.size();
        literals.resize(base + table.size());
        std::memcpy(literals.data() + base, table._table, table.size() * Interpretation::LITERAL_SIZE);
    }
}

std::optional<Utils::Vector<uint8_t>> EmitBytecode(
    const Interpretation::Code& source,
    const std::vector<RuntimeInstruction>& instructions,
    const std::vector<PreparedInlineSite>& sites,
    uint32_t callerSlotShift,
    std::vector<RewrittenCodeEdit>& edits
)
{
    Emitter::Segment output;
    auto append = [&](const uint8_t* bytes, size_t size) {
        for (size_t i = 0; i < size; ++i) {
            output.AddW8(bytes[i]);
        }
    };
    auto site = sites.begin();
    for (const auto& instruction : instructions) {
        const auto begin = output.Pos();
        const bool inlineCall = site != sites.end() && site->oldBegin == instruction.offset;
        const auto maxSize = inlineCall
            ? static_cast<uint64_t>(site->entryCode.Size()) + site->body.Size() + site->restoreCode.Size()
            : static_cast<uint64_t>(instruction.size) + (callerSlotShift != 0 ? 3 : 0);
        if (maxSize >= static_cast<uint64_t>(std::numeric_limits<int32_t>::max() - begin)) {
            return std::nullopt;
        }
        if (inlineCall) {
            append(site->entryCode.Data(), site->entryCode.Size());
            append(site->body.Data(), site->body.Size());
            append(site->restoreCode.Data(), site->restoreCode.Size());
            ++site;
        } else if (callerSlotShift != 0) {
            EmitRelocatedInstruction(output, source, instruction, callerSlotShift, 0);
        } else {
            append(source.bytecode + instruction.offset, instruction.size);
        }
        const auto size = static_cast<uint32_t>(output.Pos() - begin);
        if (inlineCall || size != instruction.size) {
            edits.push_back({ instruction.offset, instruction.offset + instruction.size, size });
        }
    }
    ASSERT(site == sites.end());
    return output.Finish();
}

std::optional<PreparedInlining> PrepareInlining(const InlinePlan& plan)
{
    if (plan.callerFuh == nullptr || plan.callSites.empty()) {
        return std::nullopt;
    }

    auto* source        = plan.expectedBytecode;
    auto* currentCaller = plan.callerFuh->bytecode.load();
    if (source == nullptr || currentCaller != source || source->version != plan.expectedVersion ||
        source->ownerFuh != plan.callerFuh || source->code.bytecodeSize >= std::numeric_limits<int32_t>::max()) {
        return std::nullopt;
    }

    const auto callerIMask = source->savedIRegs.Mask();
    const auto callerFMask = source->savedFRegs.Mask();
    auto mergedIMask = callerIMask;
    auto mergedFMask = callerFMask;
    uint32_t inlineAreaSize = 0;
    for (const auto& site : plan.callSites) {
        const auto* callee = site.expectedCalleeBytecode;
        if (callee == nullptr) {
            return std::nullopt;
        }
        inlineAreaSize = std::max(inlineAreaSize, callee->staticFrameSize);
        mergedIMask |= callee->savedIRegs.Mask();
        mergedFMask |= callee->savedFRegs.Mask();
    }
    using RegMask = std::bitset<Interpretation::NonVolatileRegs::REGS_MAX_NUM>;
    const auto addedSaveSize =
        (RegMask(mergedIMask ^ callerIMask).count() + RegMask(mergedFMask ^ callerFMask).count()) * STACK_SLOT_SIZE;
    const auto frameLayout = ExpandFrame(*source, inlineAreaSize, addedSaveSize);
    if (!frameLayout.has_value()) {
        return std::nullopt;
    }
    const auto expandedCallerStaticSize = source->staticFrameSize + frameLayout->callerSlotShift;
    if (-static_cast<int64_t>(expandedCallerStaticSize) - inlineAreaSize < std::numeric_limits<int32_t>::min()) {
        return std::nullopt;
    }

    const auto callerInstructions = DecodeRuntimeBytecode(source->code);
    std::vector<PreparedInlineSite> preparedSites;
    std::vector<RewrittenCodeEdit> edits;
    preparedSites.reserve(plan.callSites.size());
    edits.reserve(plan.callSites.size());

    std::vector<Interpretation::Literal> literals;
    ASSERT(source->code.literals->size() <= RT::LIT_TABLE_SIZE);
    AppendLiteralTable(literals, *source->code.literals);
    std::unordered_map<const Interpretation::LiteralTable*, uint32_t> literalBases { { source->code.literals, 0 } };

    uint32_t previousEnd = 0;
    for (const auto& site : plan.callSites) {
        const auto* table = site.expectedCalleeBytecode->code.literals;
        const auto tableSize = table->size();
        uint32_t literalBase = 0;
        if (tableSize != 0) {
            const auto [base, inserted] = literalBases.try_emplace(table, static_cast<uint32_t>(literals.size()));
            if (inserted) {
                if (tableSize > RT::LIT_TABLE_SIZE - literals.size()) {
                    return std::nullopt;
                }
                AppendLiteralTable(literals, *table);
            }
            literalBase = base->second;
        }

        auto prepared = PrepareSite(site, *source, callerInstructions, expandedCallerStaticSize, literalBase, literals);
        if (!prepared.has_value() || prepared->oldBegin < previousEnd) {
            return std::nullopt;
        }
        previousEnd = prepared->oldEnd;
        preparedSites.push_back(std::move(*prepared));
    }

    auto bytecode = EmitBytecode(source->code, callerInstructions, preparedSites, frameLayout->callerSlotShift, edits);
    if (!bytecode.has_value()) {
        return std::nullopt;
    }
    const auto branchPatches = PrepareBranchPatches(source->code, callerInstructions, edits, 0, literals);
    if (!branchPatches.has_value()) {
        return std::nullopt;
    }
    ApplyBranchPatches(bytecode->Data(), bytecode->Size(), *branchPatches);

    return PreparedInlining {
        .source        = source,
        .frameLayout   = *frameLayout,
        .savedIRegs    = Interpretation::NonVolatileRegs(mergedIMask),
        .savedFRegs    = Interpretation::NonVolatileRegs(mergedFMask),
        .bytecode      = std::move(*bytecode),
        .edits         = std::move(edits),
        .literals      = std::move(literals),
    };
}

void RelocateMetadataPosition(uint32_t& position, const std::vector<RewrittenCodeEdit>& edits)
{
    auto relocated = RelocateRewrittenPosition(position, edits);
    ASSERTION(relocated.has_value(), "Runtime metadata points inside a replaced call instruction");
    position = *relocated;
}

RelocatedMetadata RelocateMetadata(const PreparedInlining& prepared)
{
    const auto slotShift = prepared.frameLayout.inlineAreaSize / STACK_SLOT_SIZE;
    auto rebaseResource = [slotShift](Interpretation::Resource& resource) {
        if (!resource.IsReg()) {
            resource.idx += slotShift;
        }
    };

    // The new saves move both the slots and the static base. Only the inline area changes slot indices.
    auto gcInfo = prepared.source->gcInfo;
    for (auto& offset : gcInfo.refOffsets) {
        offset += prepared.frameLayout.inlineAreaSize;
    }
    for (auto& info : gcInfo.positionalInfo) {
        RelocateMetadataPosition(info.rewrittenPos, prepared.edits);
        for (auto& slot : info.untypedRefSlots) {
            slot += slotShift;
        }
        for (auto& [base, derived] : info.mutPairs) {
            rebaseResource(base);
            rebaseResource(derived);
        }
    }
    auto stackPtrsInfo = prepared.source->stackPtrsInfo;
    for (auto& info : stackPtrsInfo.positionalInfo) {
        RelocateMetadataPosition(info.rewrittenPos, prepared.edits);
        for (auto& resource : info.resources) {
            rebaseResource(resource);
        }
    }
    auto bcPositionsForExceptions = prepared.source->bcPositionsForExceptions;
    for (auto& [rewrittenPosition, originalPosition] : bcPositionsForExceptions) {
        static_cast<void>(originalPosition);
        RelocateMetadataPosition(rewrittenPosition, prepared.edits);
    }

    return RelocatedMetadata {
        .gcInfo                   = std::move(gcInfo),
        .stackPtrsInfo            = std::move(stackPtrsInfo),
        .bcPositionsForExceptions = std::move(bcPositionsForExceptions),
        .offsetsIndex             = prepared.source->offsetsIndex.RelocateRewrittenOffsets(prepared.edits),
    };
}

Interpretation::LiteralTable* CloneLiterals(
    const std::vector<Interpretation::Literal>& literals, Memory::Heap& heap
)
{
    const auto byteSize = literals.size() * Interpretation::LITERAL_SIZE;
    const auto allocationSize = sizeof(Interpretation::LiteralTable) + byteSize;
    auto* clone =
        static_cast<Interpretation::LiteralTable*>(heap.Allocate(allocationSize, alignof(Interpretation::LiteralTable))
        );
    clone->_byteSize = byteSize;
    if (byteSize != 0) {
        std::memcpy(clone->_table, literals.data(), byteSize);
    }
    return clone;
}

} // namespace

InlineResult Rebuild(const InlinePlan& plan, Memory::Heap& heap)
{
    auto prepared = PrepareInlining(plan);
    if (!prepared.has_value()) {
        return std::nullopt;
    }

    auto* source      = prepared->source;
    const auto bytecodeSize = prepared->bytecode.Size();
    auto* newBytecode = static_cast<uint8_t*>(heap.Allocate(bytecodeSize, alignof(uint8_t)));
    if (bytecodeSize != 0) {
        std::memcpy(newBytecode, prepared->bytecode.Data(), bytecodeSize);
    }
    auto metadata     = RelocateMetadata(*prepared);
    auto* literals    = CloneLiterals(prepared->literals, heap);

    return ExecBytecodeInfo {
        .code =
            Interpretation::Code {
                .bytecodeSize = bytecodeSize,
                .bytecode     = newBytecode,
                .literals     = literals,
            },
        .savedIRegs               = prepared->savedIRegs,
        .savedFRegs               = prepared->savedFRegs,
        .frameSize                = prepared->frameLayout.frameSize,
        .staticFrameSize          = prepared->frameLayout.staticFrameSize,
        .untypedSlotCount         = source->untypedSlotCount,
        .abiInfo                  = source->abiInfo,
        .gcInfo                   = std::move(metadata.gcInfo),
        .stackPtrsInfo            = std::move(metadata.stackPtrsInfo),
        .offsetsIndex             = std::move(metadata.offsetsIndex),
        .bcPositionsForExceptions = std::move(metadata.bcPositionsForExceptions),
        .version                  = source->version,
        .ownerFuh                 = source->ownerFuh,
    };
}

} // namespace Cbc
