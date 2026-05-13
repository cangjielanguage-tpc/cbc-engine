#pragma once

#include "RTInterface.h"

namespace GCSupport {

    void IterateFramesWithState(DYN_CJThreadSpecificDataT threadSpecificData, void (*callback)(DYN_VisitingStateT, void*), void* ctx);

    void VisitGCFrameRoots(DYN_VisitingStateT state, DYN_FrameDescT frame_desc, DYN_RootVisitorT root_visitor);

    void VisitGlobalRoots(DYN_RootVisitorT visitor);

}