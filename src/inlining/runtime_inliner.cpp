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
#include <cstring>
#include <limits>
#include <optional>
#include <unordered_map>
#include <vector>

namespace Cbc {
namespace {

using Interpretation::ExecBytecodeInfo;
using Interpretation::GCPositionalInfo;
using Interpretation::Resource;

struct PreparedInlineSite {
    const ExecBytecodeInfo* callee;
    uint32_t oldBegin;
    uint32_t oldEnd;
    RelocatedInlineBody body;
    Utils::Vector<uint8_t> entryCode;
    Utils::Vector<uint8_t> restoreCode;
};

struct PreparedSites {
    std::vector<PreparedInlineSite> sites;
    std::vector<Interpretation::Literal> literals;
};

struct AssembledBytecode {
    Utils::Vector<uint8_t> bytecode;
    std::vector<RewrittenCodeEdit> edits;
    std::vector<uint32_t> inlineStarts; // Offsets in the new bytecode, in call-site order.
};

struct RelocatedMetadata {
    Interpretation::GcInfo gcInfo;
    Interpretation::StackPtrsInfo stackPtrsInfo;
    std::vector<std::pair<uint32_t, uint32_t>> bcPositionsForExceptions;
    InstructionOffsetsIndex offsetsIndex;
};

struct PreparedInlining {
    const ExecBytecodeInfo* source;
    ExpandedFrameLayout frameLayout;
    uint16_t savedIMask;
    uint16_t savedFMask;
    Utils::Vector<uint8_t> bytecode;
    std::vector<Interpretation::Literal> literals;
    RelocatedMetadata metadata;
};

struct InlineFrameCode {
    Utils::Vector<uint8_t> entry;
    Utils::Vector<uint8_t> restore;
};

InlineFrameCode MakeInlineFrameCode(const InlineFramePlacement& placement)
{
    Emitter::Emitter entry;
    Emitter::Emitter restore;
    for (uint32_t index = 0; index < placement.iRegSaveOffsets.size(); ++index) {
        const auto offset = placement.iRegSaveOffsets[index];
        if (offset != 0) {
            const auto reg = IReg::From(index);
            entry.StoreFrame(Format::StoreAccessKind::ST_64, reg, offset);
            restore.LoadFrame(Format::LoadAccessKind::LD_64, reg, offset);
        }
    }
    for (uint32_t index = 0; index < placement.fRegSaveOffsets.size(); ++index) {
        const auto offset = placement.fRegSaveOffsets[index];
        if (offset != 0) {
            const auto reg = FReg::From(index);
            entry.StoreFrame(Format::StoreAccessKind::ST_F64, reg, offset);
            restore.LoadFrame(Format::LoadAccessKind::LD_F64, reg, offset);
        }
    }

    if (placement.localsSize != 0) {
        entry.ZeroFrame(placement.localsOffset, placement.localsSize);
        if (placement.clearOnReturn) {
            // The caller's frame-wide GC map still visits this area after the inline body returns.
            restore.ZeroFrame(placement.localsOffset, placement.localsSize);
        }
    }
    return { entry.FinishWithoutFixups(), restore.FinishWithoutFixups() };
}

std::optional<PreparedInlineSite> PrepareSite(
    const InlineCallSite& site,
    const ExecBytecodeInfo& caller,
    const std::vector<RuntimeInstruction>& callerInstructions,
    const InlineFramePlacement& placement,
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
    const auto calleeInstructions = DecodeRuntimeBytecode(callee->code);
    if (!IsSupportedBody(callee->code, calleeInstructions) || !HasSupportedInlineGC(*callee, calleeInstructions)) {
        return std::nullopt;
    }
    auto body = RelocateInlineBody(callee->code, calleeInstructions, placement.frameShift, literalBase, literals);
    if (!body.has_value()) {
        return std::nullopt;
    }

    auto frameCode = MakeInlineFrameCode(placement);
    return PreparedInlineSite {
        .callee      = callee,
        .oldBegin    = site.runtimeCallSiteOffset,
        .oldEnd      = site.runtimeCallSiteOffset + instruction->size,
        .body        = std::move(*body),
        .entryCode   = std::move(frameCode.entry),
        .restoreCode = std::move(frameCode.restore),
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

std::optional<PreparedSites> PrepareSites(
    const InlinePlan& plan,
    const ExecBytecodeInfo& source,
    const std::vector<RuntimeInstruction>& instructions,
    const InlineFramePlan& framePlan
)
{
    PreparedSites prepared;
    prepared.sites.reserve(plan.callSites.size());
    ASSERT(source.code.literals->size() <= RT::LIT_TABLE_SIZE);
    AppendLiteralTable(prepared.literals, *source.code.literals);
    std::unordered_map<const Interpretation::LiteralTable*, uint32_t> literalBases { { source.code.literals, 0 } };

    uint32_t previousEnd = 0;
    for (size_t i = 0; i < plan.callSites.size(); ++i) {
        const auto& site     = plan.callSites[i];
        const auto* table    = site.expectedCalleeBytecode->code.literals;
        const auto tableSize = table->size();
        uint32_t literalBase = 0;
        if (tableSize != 0) {
            const auto [base, inserted] =
                literalBases.try_emplace(table, static_cast<uint32_t>(prepared.literals.size()));
            if (inserted) {
                if (tableSize > RT::LIT_TABLE_SIZE - prepared.literals.size()) {
                    return std::nullopt;
                }
                AppendLiteralTable(prepared.literals, *table);
            }
            literalBase = base->second;
        }

        auto insertion =
            PrepareSite(site, source, instructions, framePlan.placements[i], literalBase, prepared.literals);
        if (!insertion.has_value() || insertion->oldBegin < previousEnd) {
            return std::nullopt;
        }
        previousEnd = insertion->oldEnd;
        prepared.sites.push_back(std::move(*insertion));
    }
    return prepared;
}

std::optional<AssembledBytecode> EmitBytecode(
    const Interpretation::Code& source,
    const std::vector<RuntimeInstruction>& instructions,
    const std::vector<PreparedInlineSite>& sites,
    uint32_t callerSlotShift,
    std::vector<Interpretation::Literal>& literals
)
{
    AssembledBytecode assembled;
    assembled.edits.reserve(sites.size());
    assembled.inlineStarts.reserve(sites.size());
    Emitter::Segment output;
    auto append = [&](const uint8_t* bytes, size_t size) {
        for (size_t i = 0; i < size; ++i) {
            output.AddW8(bytes[i]);
        }
    };
    auto site = sites.begin();
    for (const auto& instruction : instructions) {
        const auto begin      = output.Pos();
        const bool inlineCall = site != sites.end() && site->oldBegin == instruction.offset;
        const auto maxSize = inlineCall ? static_cast<uint64_t>(site->entryCode.Size()) + site->body.bytecode.Size() +
                                              site->restoreCode.Size()
                                        : static_cast<uint64_t>(instruction.size) + (callerSlotShift != 0 ? 3 : 0);
        if (maxSize >= static_cast<uint64_t>(std::numeric_limits<int32_t>::max() - begin)) {
            return std::nullopt;
        }
        if (inlineCall) {
            assembled.inlineStarts.push_back(static_cast<uint32_t>(begin));
            append(site->entryCode.Data(), site->entryCode.Size());
            append(site->body.bytecode.Data(), site->body.bytecode.Size());
            append(site->restoreCode.Data(), site->restoreCode.Size());
            ++site;
        } else if (callerSlotShift != 0) {
            EmitRelocatedInstruction(output, source, instruction, callerSlotShift, 0);
        } else {
            append(source.bytecode + instruction.offset, instruction.size);
        }
        const auto size = static_cast<uint32_t>(output.Pos() - begin);
        if (inlineCall || size != instruction.size) {
            assembled.edits.push_back({ instruction.offset, instruction.offset + instruction.size, size });
        }
    }
    ASSERT(site == sites.end());
    const auto branchPatches = PrepareBranchPatches(source, instructions, assembled.edits, 0, literals);
    if (!branchPatches.has_value()) {
        return std::nullopt;
    }
    assembled.bytecode = output.Finish();
    ApplyBranchPatches(assembled.bytecode.Data(), assembled.bytecode.Size(), *branchPatches);
    return assembled;
}

void ShiftGCSlots(GCPositionalInfo& map, uint32_t byteShift)
{
    const auto slotShift = byteShift / STACK_SLOT_SIZE;
    for (auto& slot : map.untypedRefSlots) {
        slot += slotShift;
    }
    for (auto& [base, derived] : map.mutPairs) {
        if (!base.IsReg()) {
            base.idx += slotShift;
        }
        if (!derived.IsReg()) {
            derived.idx += slotShift;
        }
    }
}

bool AppendInlineGCPoints(
    Utils::Vector<GCPositionalInfo>& points,
    const ExecBytecodeInfo& caller,
    const PreparedInlineSite& site,
    const ExpandedFrameLayout& layout,
    const InlineFramePlacement& placement,
    uint32_t inlineStart
)
{
    const auto& callee = *site.callee;
    if (callee.gcInfo.positionalInfo.Empty()) {
        return true;
    }

    const auto callEnd     = site.oldEnd;
    const auto& callerMaps = caller.gcInfo.positionalInfo;
    const auto callerMap   = std::find_if(callerMaps.begin(), callerMaps.end(), [callEnd](const auto& map) {
        return map.rewrittenPos == callEnd;
    });
    const auto& stackMaps  = caller.stackPtrsInfo.positionalInfo;
    if (callerMap == callerMaps.end() || !callerMap->paramRefSlots.Empty() || !callerMap->paramRecSlots.Empty() ||
        std::any_of(stackMaps.begin(), stackMaps.end(), [callEnd](const auto& map) {
            return map.rewrittenPos == callEnd && !map.resources.Empty();
        })) {
        return false;
    }

    auto callerRoots = *callerMap;
    ShiftGCSlots(callerRoots, layout.inlineAreaSize);
    const auto spillSlot = [&](uint32_t index) {
        const int64_t offset = static_cast<int64_t>(layout.staticFrameSize) + placement.iRegSaveOffsets[index];
        ASSERT(offset >= 0 && offset % STACK_SLOT_SIZE == 0);
        return static_cast<uint32_t>(offset / STACK_SLOT_SIZE);
    };
    const auto moveSpilledResource = [&](Resource& resource) {
        if (resource.IsReg() && placement.iRegSaveOffsets[resource.idx] != 0) {
            resource.idx = IReg::COUNT_ISA_ONLY + spillSlot(resource.idx);
        }
    };
    for (uint32_t reg = 0; reg < IReg::COUNT; ++reg) {
        const auto bit = 1U << reg;
        if ((callerRoots.regMask & bit) != 0 && placement.iRegSaveOffsets[reg] != 0) {
            callerRoots.regMask &= ~bit;
            callerRoots.untypedRefSlots.PushBack(spillSlot(reg));
        }
    }
    for (auto& [base, derived] : callerRoots.mutPairs) {
        moveSpilledResource(base);
        moveSpilledResource(derived);
    }

    for (auto map : callee.gcInfo.positionalInfo) {
        const auto position = RelocateRewrittenPosition(map.rewrittenPos, site.body.edits);
        if (!position.has_value()) {
            return false;
        }
        map.rewrittenPos = inlineStart + site.entryCode.Size() + *position;
        ShiftGCSlots(map, placement.gcSlotByteShift);
        map.regMask |= callerRoots.regMask;
        for (auto slot : callerRoots.untypedRefSlots) {
            map.untypedRefSlots.PushBack(slot);
        }
        for (const auto& pair : callerRoots.mutPairs) {
            map.mutPairs.PushBack(pair);
        }
        points.PushBack(std::move(map));
    }
    return true;
}

void RelocateMetadataPosition(uint32_t& position, const std::vector<RewrittenCodeEdit>& edits)
{
    auto relocated = RelocateRewrittenPosition(position, edits);
    ASSERTION(relocated.has_value(), "Runtime metadata points inside a replaced call instruction");
    position = *relocated;
}

std::optional<RelocatedMetadata> RelocateMetadata(
    const ExecBytecodeInfo& source,
    const InlineFramePlan& framePlan,
    const std::vector<PreparedInlineSite>& sites,
    const AssembledBytecode& assembled
)
{
    const auto& layout   = framePlan.layout;
    const auto slotShift = layout.inlineAreaSize / STACK_SLOT_SIZE;
    Interpretation::GcInfo gcInfo { .refOffsets = framePlan.inlineRefOffsets };
    for (size_t i = 0; i < sites.size(); ++i) {
        if (!AppendInlineGCPoints(
                gcInfo.positionalInfo, source, sites[i], layout, framePlan.placements[i], assembled.inlineStarts[i]
            )) {
            return std::nullopt;
        }
    }

    // Removed calls have no statepoints in the new bytecode.
    const auto wasRemovedCall = [&](uint32_t position) {
        const auto site =
            std::lower_bound(sites.begin(), sites.end(), position, [](const auto& candidate, uint32_t pos) {
                return candidate.oldEnd < pos;
            });
        return site != sites.end() && site->oldEnd == position;
    };
    // The new saves move both the slots and the static base. Only the inline area changes slot indices.
    for (auto offset : source.gcInfo.refOffsets) {
        gcInfo.refOffsets.PushBack(offset + layout.inlineAreaSize);
    }
    for (auto info : source.gcInfo.positionalInfo) {
        if (wasRemovedCall(info.rewrittenPos)) {
            continue;
        }
        RelocateMetadataPosition(info.rewrittenPos, assembled.edits);
        ShiftGCSlots(info, layout.inlineAreaSize);
        gcInfo.positionalInfo.PushBack(std::move(info));
    }
    std::sort(gcInfo.positionalInfo.begin(), gcInfo.positionalInfo.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.rewrittenPos < rhs.rewrittenPos;
    });
    Interpretation::StackPtrsInfo stackPtrsInfo;
    for (auto info : source.stackPtrsInfo.positionalInfo) {
        if (wasRemovedCall(info.rewrittenPos)) {
            continue;
        }
        RelocateMetadataPosition(info.rewrittenPos, assembled.edits);
        for (auto& resource : info.resources) {
            if (!resource.IsReg()) {
                resource.idx += slotShift;
            }
        }
        stackPtrsInfo.positionalInfo.PushBack(std::move(info));
    }
    auto bcPositionsForExceptions = source.bcPositionsForExceptions;
    for (auto& [rewrittenPosition, originalPosition] : bcPositionsForExceptions) {
        static_cast<void>(originalPosition);
        RelocateMetadataPosition(rewrittenPosition, assembled.edits);
    }

    return RelocatedMetadata {
        .gcInfo                   = std::move(gcInfo),
        .stackPtrsInfo            = std::move(stackPtrsInfo),
        .bcPositionsForExceptions = std::move(bcPositionsForExceptions),
        .offsetsIndex             = source.offsetsIndex.RelocateRewrittenOffsets(assembled.edits),
    };
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

    const auto framePlan = PlanInlineFrames(*source, plan.callSites);
    if (!framePlan.has_value()) {
        return std::nullopt;
    }
    const auto instructions = DecodeRuntimeBytecode(source->code);
    auto sites              = PrepareSites(plan, *source, instructions, *framePlan);
    if (!sites.has_value()) {
        return std::nullopt;
    }
    auto assembled =
        EmitBytecode(source->code, instructions, sites->sites, framePlan->layout.callerSlotShift, sites->literals);
    if (!assembled.has_value()) {
        return std::nullopt;
    }
    auto metadata = RelocateMetadata(*source, *framePlan, sites->sites, *assembled);
    if (!metadata.has_value()) {
        return std::nullopt;
    }

    return PreparedInlining {
        .source      = source,
        .frameLayout = framePlan->layout,
        .savedIMask  = framePlan->savedIMask,
        .savedFMask  = framePlan->savedFMask,
        .bytecode    = std::move(assembled->bytecode),
        .literals    = std::move(sites->literals),
        .metadata    = std::move(*metadata),
    };
}

Interpretation::LiteralTable* CloneLiterals(const std::vector<Interpretation::Literal>& literals, Memory::Heap& heap)
{
    const auto byteSize       = literals.size() * Interpretation::LITERAL_SIZE;
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

    auto* source            = prepared->source;
    const auto bytecodeSize = prepared->bytecode.Size();
    auto* newBytecode       = static_cast<uint8_t*>(heap.Allocate(bytecodeSize, alignof(uint8_t)));
    if (bytecodeSize != 0) {
        std::memcpy(newBytecode, prepared->bytecode.Data(), bytecodeSize);
    }
    auto& metadata = prepared->metadata;
    auto* literals = CloneLiterals(prepared->literals, heap);

    return ExecBytecodeInfo {
        .code =
            Interpretation::Code {
                .bytecodeSize = bytecodeSize,
                .bytecode     = newBytecode,
                .literals     = literals,
            },
        .savedIRegs               = Interpretation::NonVolatileRegs(prepared->savedIMask),
        .savedFRegs               = Interpretation::NonVolatileRegs(prepared->savedFMask),
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
