#pragma once

#include "RTInterface.h"
#include <optional>

namespace GCSupport {

void IterateFramesWithState(
    DYN_CJThreadSpecificData threadSpecificData, void (*callback)(DYN_VisitingState, void*), void* ctx
);

void VisitGCFrameRoots(
    DYN_VisitingState state,
    INT_FrameDesc frame_desc,
    DYN_RootVisitor rootVisitor,
    std::optional<DYN_DerivedPtrVisitor> derivedPtrVisitor = std::nullopt
);

void VisitGlobalRoots(DYN_RootVisitor visitor);

} // namespace GCSupport
