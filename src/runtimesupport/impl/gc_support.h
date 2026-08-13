#pragma once

#include "RTInterface.h"
#include "reg_table.h"
#include <optional>

namespace GCSupport {

void VisitRoot(DYN_RootVisitor rootVisitor, Placeholder ph);

void VisitMutPair(DYN_DerivedPtrVisitor derivedPtrVisitor, Placeholder basePh, Placeholder derivedPh);

void IterateFramesWithState(
    DYN_CJThreadSpecificData threadSpecificData, void (*callback)(DYN_VisitingState, void*), void* ctx
);

void VisitGCFrameRoots(
    DYN_VisitingState state,
    INT_FrameDesc frame_desc,
    DYN_RootVisitor rootVisitor,
    std::optional<DYN_DerivedPtrVisitor> derivedPtrVisitor = std::nullopt,
    bool skipPrologue                                      = false
);

void VisitGlobalRoots(DYN_RootVisitor visitor);

} // namespace GCSupport
