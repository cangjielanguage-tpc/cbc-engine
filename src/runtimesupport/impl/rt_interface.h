#pragma once

#include "RTInterface.h"

void FiberStart(DYN_CJThreadSpecificData* data);
void FiberDestroy(DYN_CJThreadSpecificData* data);
void IterateFramesWithState(
    DYN_CJThreadSpecificData threadSpecificData, void (*callback)(DYN_VisitingState, void*), void* ctx
);
void VisitFrameRootsMarking(DYN_VisitingState state, INT_FrameDesc frame_desc, DYN_RootVisitor root_visitor);
void VisitFrameRootsAdjusting(
    DYN_VisitingState state,
    INT_FrameDesc frame_desc,
    DYN_RootVisitor root_visitor,
    DYN_DerivedPtrVisitor derived_ptr_visitor
);
void VisitFrameRootsExpansion(
    DYN_VisitingState state,
    INT_FrameDesc frameDesc,
    DYN_RootVisitor stackPtrVisitor,
    DYN_DerivedPtrVisitor derivedPtrVisitor,
    DYN_RootVisitor stackAllocVisitor
);
void VisitGlobalRoots(DYN_RootVisitor visitor);
uint32_t GetFrameSize(DYN_FramePointer fp);
void FrameInfoProvider(DYN_InstructionPointer ip, DYN_FramePointer fp, INT_InterpretedFrameInfo* info);
void FrameDescProvider(INT_FunctionHandle fuh, INT_BytecodePos pos, INT_InterpretedFrameDesc* frameDesc);
