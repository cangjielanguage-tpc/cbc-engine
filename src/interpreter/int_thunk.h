#pragma once

#include <cstdint>
#include "asm_export.h"

namespace Interpretation {

enum class ThunkOp : uint8_t {
    None,
    I2ICall,
    I2CCall,
    NewObject,
    NewObjectPinned,
    NewObjectAcc,
    NewArray,
    LoadGeneric,
    GcPoint,
    Spawn,
    SpawnFuture,
    HandleException,
    ThrowImplicit,
};
static_assert((uint8_t) ThunkOp::ThrowImplicit + 1 == THUNK_OP_COUNT);
static_assert(sizeof(ThunkOp) == 1, "ThunkOp must fit in a single byte");

/// Some operations could not be called from c++ directly, because of it the interpretation loop is divided by:
/// asm part (outer loop) and c++ part (inner loop). Operations that should be invoked in asm part
/// are returned in `Thunk` from outer loop to inner loop, which would run provided operation.
/// See more details in `interpretation_loop.cpp`.
///
/// The operation to run is selected by an 8-bit `ThunkOp` index rather than a full function
/// pointer. The asm side dispatches on that index via a relative-offset jump table, which is
/// cheaper and smaller than `jmp <reg64>` with a full pointer. The single `arg` payload is
/// passed alongside the index.
struct Thunk {
    ThunkOp op;

    union {
        void* arg;
        uint64_t argUInt;
    };
};
static_assert(sizeof(Thunk) <= 16, "abi requirements");

}
