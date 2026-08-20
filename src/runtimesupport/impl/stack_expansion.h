#pragma once

#include "RTInterface.h"

namespace StackExpansion {

void VisitFrameRootsForStackPtrs(
    DYN_VisitingState state,
    INT_FrameDesc frameDesc,
    DYN_RootVisitor stackPtrVisitor,
    DYN_DerivedPtrVisitor derivedPtrVisitor,
    DYN_RootVisitor stackAllocVisitor
);

uint32_t GetFrameSize(DYN_FramePointer fp);

} // namespace StackExpansion
