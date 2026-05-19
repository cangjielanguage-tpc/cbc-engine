#pragma once

#include "RTInterface.h"

namespace GCSupport {

void IterateFramesWithState(
    DYN_CJThreadSpecificData threadSpecificData, void (*callback)(DYN_VisitingState, void*), void* ctx
);

void VisitGCFrameRoots(DYN_VisitingState state, DYN_FrameDesc frame_desc, DYN_RootVisitor root_visitor);

void VisitGlobalRoots(DYN_RootVisitor visitor);
}