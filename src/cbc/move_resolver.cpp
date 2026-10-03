#include "move_resolver.h"
#include "cbc/isa.h"
#include "utils/assertion.h"
#include "utils/function.h"
#include "utils/span.h"
#include <cstdint>

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

    static constexpr int BUFFER_SIZE = IReg::VIRT_COUNT < FReg::COUNT ? FReg::COUNT : IReg::VIRT_COUNT;
    using RegNum = uint8_t;
    RegNum iregs[IReg::VIRT_COUNT];
    RegNum fregs[FReg::COUNT];

    for (int i = 0; i < IReg::VIRT_COUNT; i++) iregs[i] = i;
    for (int i = 0; i < FReg::COUNT; i++) fregs[i] = i;

    for (auto& assignment : assignments) {
        auto dst = assignment.dst;
        auto src = assignment.src;
        auto dkind = dst.Kind();
        auto skind = src.Kind();

        if (dkind == skind && dkind == Location::IREG) {
            iregs[dst.IRegIdx()] = src.IRegIdx();
        } else if (dkind == skind && dkind == Location::FREG) {
            fregs[dst.FRegIdx()] = src.FRegIdx();
        } else if (dkind == Location::SLOT) {
            emit(dst, src);
        } else {
            if (dkind == Location::IREG && skind == Location::FREG) {
                ASSERTION(false, "mixed register kind are not allowed");
            }
            if (dkind == Location::FREG && skind == Location::IREG) {
                ASSERTION(false, "mixed register kind are not allowed");
            }
            ASSERT(skind == Location::SLOT);
            // skind == slot will be processed after cycle resolution.
        }
    }

    auto cycleResolver = [](Utils::Span<RegNum> sources, RegNum temp, Utils::Function<void(RegNum, RegNum)> const& assign) {
        uint8_t sourceCount = sources.Size();
        ASSERT(sourceCount <= BUFFER_SIZE);
        // sources is a graph: "sources[dst] = src" or edge = (sources[dst] -> dst)

        constexpr char NOT_VISITED = 0;
        constexpr char DONE        = 1;
        char state[BUFFER_SIZE]    = {NOT_VISITED};

        for (RegNum dst = 0; dst < sourceCount; dst++) {
            if (dst == sources[dst]) state[dst] = DONE;
        }

        bool changed = true; // fixed-point iteration
        while (changed) {
            changed = false;
            // Find and resolve chains.
            for (RegNum dst = 0; dst < sourceCount; dst++) {
                if (state[dst] == DONE) {
                    continue;
                }
                auto src = sources[dst];
                ASSERT(src != dst);

                // check if someone need a value of `dst`.
                bool needed = false;
                for (RegNum dstdst = 0; dstdst < sourceCount; dstdst++) {
                    if (state[dstdst] == DONE || dstdst == dst) continue;
                    if (sources[dstdst] == dst) {
                        needed = true;
                        break;
                    }
                }
                if (needed) continue;
                // safe to assign src to dst
                assign(dst, src);
                changed = true;
                state[dst] = DONE;
            }
            if (changed) continue;

            ASSERT(state[temp] == DONE);

            // Invariant: any in-progress assignment can not have a `temp` register.
            // 1. A `temp` register can not be part of cycle (due abi).
            // 2. The loop above should process all chains.
            for (RegNum dst = 0; dst < sourceCount; dst++) {
                if (state[dst] == DONE) continue;
                ASSERT(sources[dst] != temp || dst == temp);
            }

            // Cycle present and there is no chains left.
            // So, any non-done `dst` is part of cycle
            for (RegNum dst = 0; dst < sourceCount; dst++) {
                if (state[dst] == DONE) continue;
                auto src = sources[dst];
                assign(temp, src);
                sources[dst] = temp;
                changed = true;
                break;
            }
        }

        for (RegNum dst = 0; dst < sourceCount; dst++) {
            ASSERT(state[dst] == DONE);
        }
    };

    auto assignIr = [emit](RegNum dst, RegNum src) {
        emit(Location::IReg(IReg::Value(dst)), Location::IReg(IReg::Value(src)));
    };
    auto assignFr = [emit](RegNum dst, RegNum src) {
        emit(Location::FReg(FReg::Value(dst)), Location::FReg(FReg::Value(src)));
    };
    cycleResolver(Utils::Span<RegNum>(iregs, IReg::VIRT_COUNT), tempIr, assignIr);
    cycleResolver(Utils::Span<RegNum>(fregs, FReg::COUNT), TEMP_FR, assignFr);

    for (auto& assignment : assignments) {
        auto dst = assignment.dst;
        auto src = assignment.src;
        auto dkind = dst.Kind();

        if (src.Kind() == Location::SLOT && (dkind == Location::IREG || dkind == Location::FREG)) {
            emit(dst, src);
        }
    }
}

} // namespace Cbc
