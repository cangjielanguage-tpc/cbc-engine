#pragma once

namespace Interpretation {

/// Some operations could not be called from c++ directly, because of it the interpretation loop is divided by:
/// asm part (outer loop) and c++ part (inner loop). Operations that should be invoked in asm part
/// are returned in `Thunk` from outer loop to inner loop, which would run provided operation.
/// See more details in `interpretation_loop.cpp`.
struct Thunk {
    void* function;
    void* arg;
};

}
