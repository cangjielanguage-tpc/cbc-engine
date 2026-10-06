#include "move_resolver.h"
#include "cbc/isa.h"
#include "utils/assertion.h"
#include "utils/span.h"
#include <cstdint>
#include <cstdio>

namespace Cbc {

MoveResolver::MoveResolver(IReg tempIr) : tempIr(tempIr)
{
}

void MoveResolver::Clear()
{
    assignments.Clear();
}

void MoveResolver::AddMove(Location src, Location dst)
{
    ASSERT(src.Kind() != Location::NIL);
    ASSERT(dst.Kind() != Location::NIL);
    if (dst.Kind() != Location::SLOT && src.idx == dst.idx) {
        return;
    }
    assignments.PushBack(Assignment {dst, src});
}

void MoveResolver::Resolve(const Utils::Function<void(Location dst, Location src)>& emit)
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

    int iregs[IReg::VIRT_COUNT];
    int fregs[FReg::COUNT];

    for (int i = 0; i < IReg::VIRT_COUNT; i++) iregs[i] = -1;
    for (int i = 0; i < FReg::COUNT; i++) fregs[i] = -1;

    for (auto& assignment : assignments) {
        auto dst = assignment.dst;
        auto src = assignment.src;
        if (dst.Kind() != src.Kind()) continue;
        switch (dst.Kind()) {
            case Location::IREG:
                iregs[dst.IRegIdx()] = src.IRegIdx();
                break;
            case Location::FREG:
                fregs[dst.FRegIdx()] = src.FRegIdx();
                break;
            case Location::NIL:
            case Location::SLOT: {}
        }
    }

    auto cycleResolver = [](Utils::Span<int> regs, int temp) {
        ASSERT(temp < regs.Size());

    };

    struct Walker {
        Utils::Vector<Assignment> const& assignments;
        const Utils::Function<void(Location dst, Location src)>& emit;

        Location temp = NIL;
        bool cycleDetected = false;

        uint64_t visited = 0;
        uint64_t inChain = 0;
        static_assert(IReg::VIRT_COUNT + FReg::COUNT <= 64);

        bool IsVisited(Location loc) {
            auto bit = 1ULL << loc.idx;
            return (bit & visited);
        }

        bool InChain(Location loc) {
            auto bit = 1ULL << loc.idx;
            return (bit & inChain);
        }

        void Mark(Location loc) {
            auto bit = 1ULL << loc.idx;
            visited |= bit;
        }

        void ChainMark(Location loc) {
            auto bit = 1ULL << loc.idx;
            inChain |= bit;
        }

        void ChainUnmark(Location loc) {
            auto bit = 1ULL << loc.idx;
            inChain &= ~bit;
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
            ChainMark(src);

            for (auto& assignment : assignments) {
                if (assignment.src.idx != src.idx) {
                    continue;
                }
                Location dst = assignment.dst;

                if (dst.Kind() == Location::NIL) {
                    // no assignments
                } else if (dst.Kind() == Location::SLOT) {
                    emit(dst, src);
                } else if (InChain(dst)) {
                    // real cycle: dst is in the current chain
                    ASSERT(!cycleDetected);
                    ASSERT(temp.Kind() != Location::NIL);
                    emit(temp, src);
                    cycleDetected = true;
                } else if (IsVisited(dst)) {
                    // already processed, not a cycle
                    emit(dst, src);
                } else {
                    Walk(dst);
                    emit(dst, src);
                }
            }

            ChainUnmark(src);
        };
    };

    auto tempIr = Location {this->tempIr};

    Walker walker {assignments, emit};
    walker.DoWalk(tempIr);
    walker.temp = tempIr;

    for (int idx = IReg::IR1; idx < IReg::VIRT_COUNT; idx++) {
        walker.DoWalk(Location {idx});
    }

    walker.temp = NIL;
    walker.DoWalk(TEMP_FR);
    walker.temp = TEMP_FR;

    for (int idx = IReg::VIRT_COUNT + FReg::FR0; idx < IReg::VIRT_COUNT + FReg::COUNT; idx++) {
        walker.DoWalk(Location {idx});
    }

    for (auto& assignment : assignments) {
        if (assignment.src.Kind() != Location::SLOT) continue;
        emit(assignment.dst, assignment.src);
    }
}

} // namespace Cbc
