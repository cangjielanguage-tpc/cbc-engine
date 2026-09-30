#include "move_resolver.h"
#include "cbc/isa.h"
#include "utils/assertion.h"
#include <cstdint>
#include <cstdio>

namespace Cbc {

MoveResolver::MoveResolver()
{
    assignments.reserve(16);
}

void MoveResolver::Clear()
{
    assignments.clear();
}

void MoveResolver::AddMove(Location src, Location dst)
{
    if (src.idx >= assignments.size()) {
        assignments.resize(src.idx + 1, NIL);
    }
    assignments[src.idx] = dst;
}

void MoveResolver::Resolve(const std::function<void(Location dst, Location src)>& emit)
{
    // # Register resolution:
    //   x64 volatile regs IRs          - IR1-IR7
    //   x64 param-passing regs IRs     - IR1-IR6
    //   aarch64 volatile regs IRs      - IR1-IR10
    //   aarch64 param-passing regs IRs - IR1-IR8 + IR9 for sret
    //
    //   param-passing regs FRs - FR0-FR7
    //   volatile regs FRs      - FR0-FR15 // FIXME: compiler
    //
    // Choose only these ones as temp register!
    // Note that:
    // - all of them can be used as `src`
    // - only param-passing ones can be used as `dst`
    // This implies that:
    // 1. a cycle that uses all volatile registers is impossible
    // 2. it is possible to resolve register cycles in order, where `tmp` register always can be assigned out of volatile registers.
    // 3. any graph walk that starts from volatile non-param passing register would not hit a cycle!
    //
    // TODO: the algorithm for cycle breaking must:
    // - using (3) we can start graph-walking in order, where we can quickly make a register "free" - it would be possible to use it as `tmp`.
    // - the actual graph-walking must emit assignments as soon as walk stops.
    //
    // # Stack slot resolution:
    // - The `untyped slot` resource is a part of a frame that can not be used for parameter-passing.
    // - The slots that are used for param-passing are constructed dynamically by expanding frame in-place.
    //
    // Implications:
    // - the index of `dst` is pointing to the "param-passing" slot, when `src` is pointing to untyped slot.
    // - it is impossible to have a cycle that have stack slot.
    //
    // # Additional assumptions:
    // - There is no (FP, IR) or (IR, FP) assignments.
    // - Each `dst` can be target of not more than one assignment.

    struct Walker {
        std::vector<Location> const& assignments;
        const std::function<void(Location dst, Location src)>& emit;

        Location temp = NIL;
        bool cycleDetected = false;

        uint64_t visited = 0;
        static_assert(IReg::VIRT_COUNT + FReg::COUNT <= 64);

        bool IsVisited(Location loc) {
            auto bit = 1ULL << loc.idx;
            return (bit & visited);
        }

        void Mark(Location loc) {
            auto bit = 1ULL << loc.idx;
            visited |= bit;
        }

        void DoWalk(Location loc) {
            if (IsVisited(loc)) {
                return;
            }

            cycleDetected = false;
            Walk(loc);
            if (cycleDetected) {
                ASSERT(temp.Kind() != Location::NIL);
                emit(loc, temp);
                cycleDetected = false;
            }
        }

        void Walk(Location src) {
            ASSERT(src.Kind() == Location::FREG || src.Kind() == Location::IREG);
            ASSERT(!IsVisited(src));
            Mark(src);

            Location dst = src.idx < assignments.size() ? assignments[src.idx] : NIL;

            if (dst.Kind() == Location::NIL) {
                // no assignments
            } else if (IsVisited(dst)) {
                // cycle detected.
                ASSERT(!cycleDetected);
                ASSERT(temp.Kind() != Location::NIL);
                emit(temp, src);
                cycleDetected = true;
            } else {
                Walk(dst);
                emit(dst, src);
            }
        };
    };

    printf("Assignments size: %zu\n", assignments.size());

    Walker walker {assignments, emit};
    walker.DoWalk(TEMP_IR);
    walker.temp = TEMP_IR;

    for (int idx = IReg::IR1; idx < IReg::VIRT_COUNT; idx++) {
        walker.DoWalk(Location {idx});
    }

    walker.temp = NIL;
    walker.DoWalk(TEMP_FR);
    walker.temp = TEMP_FR;

    for (int idx = IReg::VIRT_COUNT + FReg::FR0; idx < IReg::VIRT_COUNT + FReg::COUNT; idx++) {
        walker.DoWalk(Location {idx});
    }

    for (int idx = IReg::VIRT_COUNT + FReg::COUNT; idx < assignments.size(); idx++) {
        auto dst = assignments[idx];
        if (dst.Kind() != Location::NIL) {
            emit(dst, Location{idx});
        }
    }
}

} // namespace Cbc
