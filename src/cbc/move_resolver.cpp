#include "move_resolver.h"
#include "cbc/isa.h"
#include "utils/assertion.h"
#include "utils/function.h"
#include "utils/span.h"
#include <cstdint>

namespace Cbc {

MoveResolver::MoveResolver(IReg tempIr) : tempIr(tempIr) {}

void MoveResolver::Clear() { assignments.Clear(); }

void MoveResolver::AddMove(Location src, Location dst)
{
    ASSERT(src.Kind() != Location::NIL);
    ASSERT(dst.Kind() != Location::NIL);
    if (dst.Kind() != Location::SLOT && src.idx == dst.idx) {
        return;
    }
    assignments.PushBack(Assignment { dst, src });
}

// Dispatches a single (dst, src) move to the matching MoveEmitter method.
static void EmitMove(MoveEmitter& emitter, Location dst, Location src)
{
    auto dkind = dst.Kind();
    auto skind = src.Kind();
    if (skind == Location::IREG && dkind == Location::IREG) {
        emitter.AssignIReg(IReg::From(dst.IRegIdx()), IReg::From(src.IRegIdx()));
    } else if (skind == Location::FREG && dkind == Location::FREG) {
        emitter.AssignFReg(FReg::From(dst.FRegIdx()), FReg::From(src.FRegIdx()));
    } else if (skind == Location::SLOT && dkind == Location::SLOT) {
        emitter.AssignParamSlot(dst.SlotIdx(), src.SlotIdx());
    } else if (skind == Location::FREG && dkind == Location::SLOT) {
        emitter.AssignSlotFromFReg(dst.SlotIdx(), FReg::From(src.FRegIdx()));
    } else if (skind == Location::IREG && dkind == Location::SLOT) {
        emitter.AssignSlotFromIReg(dst.SlotIdx(), IReg::From(src.IRegIdx()));
    } else if (skind == Location::SLOT && dkind == Location::IREG) {
        emitter.AssignIRegFromSlot(IReg::From(dst.IRegIdx()), src.SlotIdx());
    } else if (skind == Location::SLOT && dkind == Location::FREG) {
        emitter.AssignFRegFromSlot(FReg::From(dst.FRegIdx()), src.SlotIdx());
    } else {
        ASSERT(false);
    }
}

bool MoveResolver::Resolve(MoveEmitter& emitter)
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
    // Choose only these as temp registers!
    // Note that:
    // - all of them can be used as `src`
    // - only param-passing ones can be used as `dst`
    // This implies that:
    // 1. a cycle that uses all volatile registers is impossible
    // 2. it is possible to resolve register cycles in order, where `tmp` register can always be assigned out of
    // volatile registers.
    // 3. any graph walk that starts from volatile non-param passing register would not hit a cycle!
    //
    // # Stack slot resolution:
    // - The `untyped slot` resource is a part of a frame that cannot be used for parameter-passing.
    // - The slots that are used for param-passing are constructed dynamically by expanding frame in-place.
    //
    // Implications:
    // - the index of `dst` points to the "param-passing" slot, when `src` points to an untyped slot.
    // - it is impossible to have a cycle that has a stack slot.
    //
    // # Additional assumptions:
    // - There is no (FP, IR) or (IR, FP) assignments.
    // - Each `dst` can be target of not more than one assignment.
    //
    // # Algorithm:
    // Build a mapping `regs[dst] = src` for each register kind (IR, FR).
    // Then run a fixed-point iteration over the mapping:
    //
    // 1. Mark nodes where `src == dst` as DONE (no-op).
    // 2. Repeat until no changes:
    //    a. For each non-DONE `dst`, check if any other non-DONE node
    //       references `dst` as its source. If yes, `dst` is part of a
    //       cycle or a chain that depends on it — skip.
    //    b. If no one references `dst`, it is safe to emit `dst = src`
    //       and mark it DONE.
    //    c. If no progress was made, the remaining non-DONE nodes form
    //       a cycle. Break it by emitting `temp = src` for one node and
    //       rewriting its source to `temp`. The `temp` register is
    //       guaranteed not to be in the cycle (by ABI constraints).
    // 3. When all nodes are DONE, the mapping is fully resolved.
    //
    // Complexity: O(n^2)
    // O(n) iterations in the worst case (one node resolved per iteration for a single cycle).
    // No heap allocations — only stack-allocated arrays.

    static constexpr int BUFFER_SIZE = IReg::VIRT_COUNT < FReg::COUNT ? FReg::COUNT : IReg::VIRT_COUNT;
    using RegNum                     = uint8_t;
    RegNum iregs[IReg::VIRT_COUNT];
    RegNum fregs[FReg::COUNT];

    for (int i = 0; i < IReg::VIRT_COUNT; i++)
        iregs[i] = i;
    for (int i = 0; i < FReg::COUNT; i++)
        fregs[i] = i;

    for (auto& assignment : assignments) {
        auto dst   = assignment.dst;
        auto src   = assignment.src;
        auto dkind = dst.Kind();
        auto skind = src.Kind();

        if (dkind == skind && dkind == Location::IREG) {
            iregs[dst.IRegIdx()] = src.IRegIdx();
        } else if (dkind == skind && dkind == Location::FREG) {
            fregs[dst.FRegIdx()] = src.FRegIdx();
        } else if (dkind == Location::SLOT) {
            // Param slots cannot form any chains,
            // to handle them emit all needed stores immediately before registers shuffle.
            EmitMove(emitter, dst, src);
        } else {
            if (dkind == Location::IREG && skind == Location::FREG) {
                return false;
            }
            if (dkind == Location::FREG && skind == Location::IREG) {
                return false;
            }
            ASSERT(skind == Location::SLOT);
            // This case will be processed after registers shuffle.
        }
    }

    auto cycleResolver =
        [](Utils::Span<RegNum> sources, RegNum temp, Utils::Function<void(RegNum, RegNum)> const& assign) {
            uint8_t regCount = sources.Size();
            ASSERT(regCount <= BUFFER_SIZE);
            // sources is a graph: "sources[dst] = src" or edge = (sources[dst] -> dst)

            constexpr char NOT_VISITED = 0;
            constexpr char DONE        = 1;
            char state[BUFFER_SIZE]    = { NOT_VISITED };

            for (RegNum dst = 0; dst < regCount; dst++) {
                if (dst == sources[dst])
                    state[dst] = DONE;
            }

            bool changed = true; // fixed-point iteration
            while (changed) {
                changed = false;
                // Find and resolve chains.
                for (RegNum dst = 0; dst < regCount; dst++) {
                    if (state[dst] == DONE) {
                        continue;
                    }
                    auto src = sources[dst];
                    ASSERT(src != dst);

                    // check if someone needs a value of `dst`.
                    bool needed = false;
                    for (RegNum dstdst = 0; dstdst < regCount; dstdst++) {
                        if (state[dstdst] == DONE || dstdst == dst)
                            continue;
                        if (sources[dstdst] == dst) {
                            needed = true;
                            break;
                        }
                    }
                    if (needed)
                        continue;
                    // safe to assign src to dst
                    assign(dst, src);
                    changed    = true;
                    state[dst] = DONE;
                }
                if (changed)
                    continue;

                ASSERT(state[temp] == DONE);

                // Invariant: any in-progress assignment cannot have a `temp` register.
                // 1. A `temp` register cannot be part of a cycle (due to the ABI).
                // 2. The loop above should process all chains.
                for (RegNum dst = 0; dst < regCount; dst++) {
                    if (state[dst] == DONE)
                        continue;
                    ASSERT(sources[dst] != temp || dst == temp);
                    // The loop will be optimized out.
                }

                // A cycle is present and there are no chains left.
                // So, any non-done `dst` is part of a cycle.
                for (RegNum dst = 0; dst < regCount; dst++) {
                    if (state[dst] == DONE)
                        continue;
                    auto src = sources[dst];
                    assign(temp, src);
                    sources[dst] = temp;
                    changed      = true;
                    break;
                }
            }

            for (RegNum dst = 0; dst < regCount; dst++) {
                ASSERT(state[dst] == DONE);
                // The loop will be optimized out.
            }
        };

    auto assignIr = [&emitter](RegNum dst, RegNum src) { emitter.AssignIReg(IReg::From(dst), IReg::From(src)); };
    auto assignFr = [&emitter](RegNum dst, RegNum src) { emitter.AssignFReg(FReg::From(dst), FReg::From(src)); };
    cycleResolver(Utils::Span<RegNum>(iregs, IReg::VIRT_COUNT), tempIr, assignIr);
    cycleResolver(Utils::Span<RegNum>(fregs, FReg::COUNT), TEMP_FR, assignFr);

    for (auto& assignment : assignments) {
        auto dst   = assignment.dst;
        auto src   = assignment.src;
        auto dkind = dst.Kind();

        if (src.Kind() == Location::SLOT && dkind == Location::IREG) {
            emitter.AssignIRegFromSlot(IReg::From(dst.IRegIdx()), src.SlotIdx());
        } else if (src.Kind() == Location::SLOT && dkind == Location::FREG) {
            emitter.AssignFRegFromSlot(FReg::From(dst.FRegIdx()), src.SlotIdx());
        }
    }
    return true;
}

} // namespace Cbc
