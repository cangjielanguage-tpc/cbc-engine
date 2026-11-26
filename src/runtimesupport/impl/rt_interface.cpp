#include "rt_interface.h"

#include "entrypoint.h"
#include "exception_handling.h"
#include "gc_support.h"
#include "interpreter/ectype.h"
#include "stack_expansion.h"
#include "utils/assertion.h"
#include "utils/logger.h"
#include "utils/ostream.h"
#include "utils/rt_logger.h"

void FiberStart(DYN_CJThreadSpecificData* data) { *data = nullptr; }

void FiberDestroy(DYN_CJThreadSpecificData* data)
{
    if (*data == nullptr) {
        // ectype wasn't initialized for this fiber, nothing to do here
        return;
    }

    LOG_INFO(RTSupport::Log::rt, "fiber destroy, fsd: {}, ectype: {}", Stream::Hex(data), Stream::Hex(*data));
    delete static_cast<Interpretation::Ectype*>(*data)->Checked();
}

void IterateFramesWithState(
    DYN_CJThreadSpecificData threadSpecificData, void (*callback)(DYN_VisitingState, void*), void* ctx
)
{
    if (IsEngineLoaded()) {
        GCSupport::IterateFramesWithState(threadSpecificData, callback, ctx);
    }
}

void VisitFrameRootsMarking(DYN_VisitingState state, INT_FrameDesc frame_desc, DYN_RootVisitor root_visitor)
{
    if (IsEngineLoaded()) {
        GCSupport::VisitGCFrameRoots(state, frame_desc, root_visitor);
    }
}

void VisitFrameRootsAdjusting(
    DYN_VisitingState state,
    INT_FrameDesc frame_desc,
    DYN_RootVisitor root_visitor,
    DYN_DerivedPtrVisitor derived_ptr_visitor
)
{
    if (IsEngineLoaded()) {
        GCSupport::VisitGCFrameRoots(state, frame_desc, root_visitor, derived_ptr_visitor);
    }
}

void VisitFrameRootsExpansion(
    DYN_VisitingState state,
    INT_FrameDesc frameDesc,
    DYN_RootVisitor stackPtrVisitor,
    DYN_DerivedPtrVisitor derivedPtrVisitor,
    DYN_RootVisitor stackAllocVisitor
)
{
    if (IsEngineLoaded()) {
        StackExpansion::VisitFrameRootsForStackPtrs(
            state, frameDesc, stackPtrVisitor, derivedPtrVisitor, stackAllocVisitor
        );
    }
}

void VisitGlobalRoots(DYN_RootVisitor visitor)
{
    if (IsEngineLoaded()) {
        GCSupport::VisitGlobalRoots(visitor);
    }
}

uint32_t GetFrameSize(DYN_FramePointer fp)
{
    ASSERTION(IsEngineLoaded(), "GetFrameSize cannot be called if there are no interpreter frames");
    return StackExpansion::GetFrameSize(fp);
}

void FrameInfoProvider(DYN_InstructionPointer ip, DYN_FramePointer fp, INT_InterpretedFrameInfo* info)
{
    if (IsEngineLoaded()) {
        EHSupport::FrameInfoProvider(ip, fp, info);
    }
}

void FrameDescProvider(INT_FunctionHandle fuh, INT_BytecodePos pos, INT_InterpretedFrameDesc* frameDesc)
{
    if (IsEngineLoaded()) {
        EHSupport::FrameDescProvider(fuh, pos, frameDesc);
    }
}
