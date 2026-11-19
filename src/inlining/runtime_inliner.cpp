#include "runtime_inliner.h"

#include "cbc/offsets_index.h"
#include "inlining/inline_plan.h"
#include "inlining/runtime_bytecode.h"
#include "interpreter/function_handle.h"
#include "interpreter/literals.h"
#include "utils/assertion.h"
#include "utils/heap.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <optional>
#include <vector>

namespace Cbc {
namespace {

using Interpretation::ExecBytecodeInfo;

struct PreparedInlineSite {
    const Interpretation::Code* calleeCode;
    uint32_t oldBegin;
    uint32_t oldEnd;
    uint32_t inlineBodySize;
};

struct PreparedInlining {
    const ExecBytecodeInfo* source;
    std::vector<PreparedInlineSite> sites;
    std::vector<RewrittenCodeEdit> edits;
    std::vector<BranchPatch> branchPatches;
    uint32_t bytecodeSize;
};

struct RelocatedMetadata {
    Interpretation::GcInfo gcInfo;
    Interpretation::StackPtrsInfo stackPtrsInfo;
    std::vector<std::pair<uint32_t, uint32_t>> bcPositionsForExceptions;
    InstructionOffsetsIndex offsetsIndex;
};

std::optional<PreparedInlineSite> PrepareSite(
    const InlineCallSite& site,
    const ExecBytecodeInfo& caller,
    const std::vector<RuntimeInstruction>& callerInstructions
)
{
    const auto instruction = std::lower_bound(
        callerInstructions.begin(),
        callerInstructions.end(),
        site.runtimeCallSiteOffset,
        [](const auto& candidate, uint32_t offset) { return candidate.offset < offset; }
    );
    if (instruction == callerInstructions.end() || instruction->offset != site.runtimeCallSiteOffset ||
        instruction->opcode != RT::Opcode::DIRECT_CALL_2I || instruction->size != RT::B3xi12::SIZE) {
        return std::nullopt;
    }
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
    if (callee == nullptr || currentCallee != callee || callee->version != site.expectedCalleeVersion ||
        callee->ownerFuh != site.calleeFuh) {
        return std::nullopt;
    }
    if (!callee->gcInfo.positionalInfo.Empty() || !callee->stackPtrsInfo.positionalInfo.Empty() ||
        !IsSupportedBody(callee->code)) {
        return std::nullopt;
    }

    const auto calleeInstructions = DecodeRuntimeBytecode(callee->code);
    const auto& terminalReturn    = calleeInstructions.back();
    ASSERT(terminalReturn.opcode == RT::Opcode::RET);
    ASSERT(terminalReturn.offset + terminalReturn.size == callee->code.bytecodeSize);

    return PreparedInlineSite {
        .calleeCode     = &callee->code,
        .oldBegin       = site.runtimeCallSiteOffset,
        .oldEnd         = site.runtimeCallSiteOffset + instruction->size,
        .inlineBodySize = terminalReturn.offset,
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
        source->ownerFuh != plan.callerFuh || source->code.bytecodeSize > std::numeric_limits<uint32_t>::max()) {
        return std::nullopt;
    }

    const auto callerInstructions = DecodeRuntimeBytecode(source->code);
    std::vector<PreparedInlineSite> preparedSites;
    std::vector<RewrittenCodeEdit> edits;
    preparedSites.reserve(plan.callSites.size());
    edits.reserve(plan.callSites.size());

    uint32_t previousEnd    = 0;
    int64_t newBytecodeSize = static_cast<int64_t>(source->code.bytecodeSize);
    for (const auto& site : plan.callSites) {
        auto prepared = PrepareSite(site, *source, callerInstructions);
        if (!prepared.has_value()) {
            return std::nullopt;
        }
        if (prepared->oldBegin < previousEnd) {
            return std::nullopt;
        }
        previousEnd = prepared->oldEnd;

        newBytecodeSize += static_cast<int64_t>(prepared->inlineBodySize) -
                           static_cast<int64_t>(prepared->oldEnd - prepared->oldBegin);
        if (newBytecodeSize < 0 || newBytecodeSize > std::numeric_limits<uint32_t>::max()) {
            return std::nullopt;
        }

        edits.push_back(RewrittenCodeEdit {
            .oldBegin = prepared->oldBegin,
            .oldEnd   = prepared->oldEnd,
            .newSize  = prepared->inlineBodySize,
        });
        preparedSites.push_back(*prepared);
    }

    auto branchPatches = PrepareBranchPatches(source->code, callerInstructions, edits);
    if (!branchPatches.has_value()) {
        return std::nullopt;
    }

    return PreparedInlining {
        .source        = source,
        .sites         = std::move(preparedSites),
        .edits         = std::move(edits),
        .branchPatches = std::move(*branchPatches),
        .bytecodeSize  = static_cast<uint32_t>(newBytecodeSize),
    };
}

uint8_t* EmitBytecode(const PreparedInlining& prepared, Memory::Heap& heap)
{
    auto* source       = prepared.source;
    auto* newBytecode  = static_cast<uint8_t*>(heap.Allocate(prepared.bytecodeSize, alignof(uint8_t)));
    uint32_t oldCursor = 0;
    uint32_t newCursor = 0;
    for (const auto& site : prepared.sites) {
        const auto prefixSize = site.oldBegin - oldCursor;
        std::memcpy(newBytecode + newCursor, source->code.bytecode + oldCursor, prefixSize);
        newCursor += prefixSize;

        std::memcpy(newBytecode + newCursor, site.calleeCode->bytecode, site.inlineBodySize);
        newCursor += site.inlineBodySize;
        oldCursor  = site.oldEnd;
    }
    const auto suffixSize = static_cast<uint32_t>(source->code.bytecodeSize) - oldCursor;
    std::memcpy(newBytecode + newCursor, source->code.bytecode + oldCursor, suffixSize);
    newCursor += suffixSize;
    ASSERT(newCursor == prepared.bytecodeSize);
    ApplyBranchPatches(newBytecode, prepared.bytecodeSize, prepared.branchPatches);
    return newBytecode;
}

void RelocateMetadataPosition(uint32_t& position, const std::vector<RewrittenCodeEdit>& edits)
{
    auto relocated = RelocateRewrittenPosition(position, edits);
    ASSERTION(relocated.has_value(), "Runtime metadata points inside a replaced call instruction");
    position = *relocated;
}

RelocatedMetadata RelocateMetadata(const PreparedInlining& prepared)
{
    auto gcInfo = prepared.source->gcInfo;
    for (auto& info : gcInfo.positionalInfo) {
        RelocateMetadataPosition(info.rewrittenPos, prepared.edits);
    }
    auto stackPtrsInfo = prepared.source->stackPtrsInfo;
    for (auto& info : stackPtrsInfo.positionalInfo) {
        RelocateMetadataPosition(info.rewrittenPos, prepared.edits);
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

Interpretation::LiteralTable* CloneLiterals(const Interpretation::LiteralTable& source, Memory::Heap& heap)
{
    const auto allocationSize = sizeof(Interpretation::LiteralTable) + source._byteSize;
    auto* clone =
        static_cast<Interpretation::LiteralTable*>(heap.Allocate(allocationSize, alignof(Interpretation::LiteralTable))
        );
    clone->_byteSize = source._byteSize;
    std::memcpy(clone->_table, source._table, source._byteSize);
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
    auto* newBytecode = EmitBytecode(*prepared, heap);
    auto metadata     = RelocateMetadata(*prepared);
    auto* literals    = CloneLiterals(*source->code.literals, heap);

    return ExecBytecodeInfo {
        .code =
            Interpretation::Code {
                .bytecodeSize = prepared->bytecodeSize,
                .bytecode     = newBytecode,
                .literals     = literals,
            },
        .savedIRegs               = source->savedIRegs,
        .savedFRegs               = source->savedFRegs,
        .frameSize                = source->frameSize,
        .staticFrameSize          = source->staticFrameSize,
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
