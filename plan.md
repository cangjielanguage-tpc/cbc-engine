# Plan: Resolving Parallel Move Conflicts in `EmitCallArgs`

## Problem

`EmitCallArgs` emits a sequence of `mov` instructions to place call arguments into ABI
parameter registers. Each move has a **source** (IReg, FReg, or stack slot) and a
**destination** (IReg, FReg, or stack slot). When executed sequentially, a later move
can overwrite a source that an earlier move hasn't consumed yet.

Example:
```
x, y, z = y, x, x
```
Sequential execution `x = y; y = x; z = x` is wrong: after `x = y`, the original `x`
is lost. Correct result requires a temporary:
```
tmp = x
x = y
y = tmp
z = tmp
```

## Why This Matters Here

In the new call format, the argument list encodes sources that are the *current*
register/slot values at the callsite. The compiler (front-end) emits these lists
assuming simultaneous semantics. If the rewriter naively emits moves in list order,
overlapping src/dst pairs produce corrupted arguments.

## Solution: Cycle Detection + Topological Sort

This is the standard "parallel move resolution" algorithm from compiler theory
(see e.g. the phi-elimination phase in LLVM, or the "register move ordering"
problem in the Dragon Book, §10.5).

### Algorithm

1. **Build a dependency graph.**
   - Nodes: indices into the move list (0..N-1).
   - Edge `i → j` exists if `src_i` is the same resource as `dst_j` (i.e., move `i`
     reads a value that move `j` overwrites). Move `i` must execute **before** move `j`.

2. **Find cycles.**
   - Use DFS (colored: white/gray/black) to detect back-edges.
   - Each cycle is a set of moves that mutually depend on each other.

3. **Break cycles with a temporary.**
   - For each cycle, select one node (e.g., the first in the cycle).
   - Before the cycle's moves execute, save that node's source value to a temporary.
   - After the cycle's moves, restore from the temporary to the final destination.
   - Temporary options (in preference order):
     - `IR_ACC` (the interpreter's scratch register, already used for temps).
     - A spare stack slot (one of the pre-allocated frame slots).
   - Breaking one edge per cycle is sufficient to make the graph a DAG.

4. **Topologically sort the DAG.**
   - Use Kahn's algorithm (BFS on in-degree-0 nodes).
   - The resulting order is a valid sequential execution.

5. **Emit moves in topological order**, with the temporary save/restore around
   each broken cycle.

### Complexity

- N = number of arguments (typically small: 1–20).
- Graph has at most N(N-1) edges (but in practice far fewer).
- Cycle detection: O(N + E) = O(N²) worst case.
- Topological sort: O(N + E).
- For N ≤ 20, this is negligible.

### Implementation in `isa_rewriter.cpp`

Add a helper function `ResolveMoveOrder` that:

```cpp
// Input: vector of (source, destination) pairs
// Output: vector of move indices in execution order, plus
//         a list of (tempSave, tempRestore) pairs for cycle breaks

struct MovePlan {
    std::vector<uint32_t> order;        // topological order of move indices
    std::vector<std::pair<uint32_t, uint32_t>> cycleBreaks; // (moveIdx, tempSlotIdx)
};

MovePlan ResolveMoveOrder(const std::vector<CallSource>& sources,
                           const std::vector<Dst>& destinations);
```

Where `Dst` is a new small struct mirroring `CallSource` for destinations:
```cpp
struct Dst {
    enum Kind { IREG, FREG, SLOT };
    Kind kind;
    uint32_t idx;
};
```

The `EmitCallArgs` function will:
1. Decode all sources and destinations from the argument list + signature.
2. Call `ResolveMoveOrder` to get the execution plan.
3. Emit `StoreFrame`/`LoadFrame` for cycle-break temporaries.
4. Emit moves in topological order.

### Temporary Resource Selection

- **Preferred**: `IR_ACC` — already the interpreter's scratch register.
  Available for use between the save and restore of each cycle break.
- **Fallback**: A dedicated stack slot. Since we pre-size the frame with
  `maxCalleeStackArgsCount * 8` bytes, we can carve out one slot as a permanent
  temp. This requires +8 bytes in the frame size.

For simplicity and to avoid interference with the ABI parameter slots, use
**IR_ACC** for all cycle breaks. The rewriter already uses IR_ACC as a scratch
register in other contexts (e.g., `EmitSourceToStackSlot` uses it for slot→slot
copies).

### Edge Cases

- **No conflicts** (most common): the graph is already a DAG, no temp needed.
  Topological sort gives the same order as the original list (or a valid
  reordering).
- **Self-loop** (`x = x`): trivially removable (no-op move).
- **Multiple cycles**: each gets its own break; IR_ACC is reused sequentially
  (save cycle 1, execute cycle 1, restore; save cycle 2, execute cycle 2, restore).
- **Slot-to-slot conflicts**: same logic applies; the temp is IR_ACC (load from
  slot, store to slot).

### What NOT to Do

- Do not try to minimize the number of temporaries (one per cycle is optimal
  and sufficient).
- Do not attempt register renaming (the sources/destinations are fixed by the
  ABI; we cannot change which register holds which parameter).
- Do not reorder moves across the sret/mut/receiver special handling — those
  are emitted first and their destinations (IR1, IR2, etc.) are not in the
  "normal parameter" move set.

### Test Strategy

Unit tests in `test/` can verify:
- No-conflict case: moves emitted in original order.
- Simple swap: `x = y; y = x` → uses temp.
- 3-cycle: `x = y; y = z; z = x` → uses temp.
- Mixed IReg/FReg: no cross-type conflicts (IReg dst ≠ FReg src).
- Slot conflicts: slot→slot with overlap.
