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
    assignments.PushBack(Assignment { dst, src, 0, Format::LoadAccessKind::LD_64, false });
}

void MoveResolver::AddLoadFromAddr(Location srcAddr, int32_t offset, Format::LoadAccessKind ldk, Location dst)
{
    ASSERT(srcAddr.Kind() == Location::IREG || srcAddr.Kind() == Location::SLOT);
    ASSERT(dst.Kind() != Location::NIL);
    assignments.PushBack(Assignment { dst, srcAddr, offset, ldk, true });
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

// Dispatches a single load-from-address to the matching MoveEmitter method.
static void EmitLoadFromAddr(
    MoveEmitter& emitter, Location dst, Location srcAddr, int32_t offset, Format::LoadAccessKind ldk
)
{
    switch (dst.Kind()) {
        case Location::IREG: emitter.AssignIRegFromAddr(IReg::From(dst.IRegIdx()), srcAddr, offset, ldk); break;
        case Location::FREG: emitter.AssignFRegFromAddr(FReg::From(dst.FRegIdx()), srcAddr, offset, ldk); break;
        case Location::SLOT: emitter.AssignSlotFromAddr(dst.SlotIdx(), srcAddr, offset, ldk); break;
        default:             ASSERT(false);
    }
}

// Graph node: either a move (`dst = src`) or a load (`dst = *(src + offset)`).
// For both kinds `src` is what the node reads: the source register or the load address.
struct AssignmentInfo {
    bool isLoad { false };

    union {
        struct {
            Location src;
        } moveInfo;

        struct {
            Location src;
            int32_t offset;
            Format::LoadAccessKind ldk;
        } loadInfo;
    };

    Location Src() { return isLoad ? loadInfo.src : moveInfo.src; }

    void SetSrc(Location src)
    {
        if (isLoad) {
            loadInfo.src = src;
        } else {
            moveInfo.src = src;
        }
    }
};

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
    // # Loads from address (`fromAddr` assignments):
    // - modeled as nodes that read the register holding the address, so the
    //   address cannot be rewritten before the load is emitted;
    // - loads into param slots are emitted immediately (slots are never read
    //   back, and the address is read before the register shuffle);
    // - loads whose address lives in an untyped slot are emitted after the
    //   register shuffle, like plain slot->reg moves: the slot is immutable
    //   and everything reading the destination register goes first;
    // - loads into registers live in the graph of their destination kind.
    //   The freg graph is resolved first, so freg loads read their ireg
    //   addresses before the ireg shuffle touches them.
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
    using RegNum = uint8_t;

    AssignmentInfo iregNodes[IReg::VIRT_COUNT] = {};
    AssignmentInfo fregNodes[FReg::COUNT]      = {};

    for (int i = 0; i < IReg::VIRT_COUNT; i++)
        iregNodes[i] = AssignmentInfo { .moveInfo = { Location::IReg(IReg::From(i)) } };
    for (int i = 0; i < FReg::COUNT; i++)
        fregNodes[i] = AssignmentInfo { .moveInfo = { Location::FReg(FReg::From(i)) } };

    for (auto& assignment : assignments) {
        auto dst   = assignment.dst;
        auto src   = assignment.src;
        auto dkind = dst.Kind();
        auto skind = src.Kind();

        if (assignment.fromAddr) {
            // Handle loads.

            if (dkind == Location::SLOT) {
                // Param slots cannot form any chains,
                // to handle them emit all needed loads immediately before registers shuffle.
                EmitLoadFromAddr(emitter, assignment.dst, assignment.src, assignment.offset, assignment.ldk);
                continue;
            }
            if (skind == Location::SLOT) {
                // This case will be processed after registers shuffle.
                continue;
            }
            if (dkind == Location::IREG) {
                iregNodes[dst.IRegIdx()] = AssignmentInfo {
                    .isLoad   = true,
                    .loadInfo = { src, assignment.offset, assignment.ldk },
                };
            } else {
                ASSERT(dkind == Location::FREG);
                fregNodes[dst.FRegIdx()] = AssignmentInfo {
                    .isLoad   = true,
                    .loadInfo = { src, assignment.offset, assignment.ldk },
                };
            }
        } else {
            // Handle moves.

            if (dkind == skind && dkind == Location::IREG) {
                iregNodes[dst.IRegIdx()] = AssignmentInfo { .moveInfo = { src } };
            } else if (dkind == skind && dkind == Location::FREG) {
                fregNodes[dst.FRegIdx()] = AssignmentInfo { .moveInfo = { src } };
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
    }

    auto cycleResolver = [](Utils::Span<AssignmentInfo> nodes,
                            Location temp,
                            Utils::Function<void(RegNum, Location)> const& assign) {
        uint8_t regCount = nodes.Size();
        ASSERT(regCount <= BUFFER_SIZE);
        ASSERT(temp.Kind() == Location::IREG || temp.Kind() == Location::FREG);
        // The graph is: "nodes[dst].Src() -> dst"

        RegNum tempIdx = temp.Kind() == Location::IREG ? temp.IRegIdx() : temp.FRegIdx();
        auto dstLoc    = [&temp](RegNum dst) {
            return temp.Kind() == Location::IREG ? Location::IReg(IReg::From(dst)) : Location::FReg(FReg::From(dst));
        };

        constexpr char NOT_VISITED = 0;
        constexpr char DONE        = 1;
        char state[BUFFER_SIZE]    = { NOT_VISITED };

        for (RegNum dst = 0; dst < regCount; dst++) {
            if (nodes[dst].Src() == dstLoc(dst) && !nodes[dst].isLoad)
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
                auto src = nodes[dst].Src();
                ASSERT(src != dstLoc(dst) || nodes[dst].isLoad);

                // check if someone needs a value of `dst`.
                bool needed = false;
                for (RegNum dstdst = 0; dstdst < regCount; dstdst++) {
                    if (state[dstdst] == DONE || dstdst == dst)
                        continue;
                    if (nodes[dstdst].Src() == dstLoc(dst)) {
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

            ASSERT(state[tempIdx] == DONE);

            // Invariant: any in-progress assignment cannot have a `temp` register.
            // 1. A `temp` register cannot be part of a cycle (due to the ABI).
            // 2. The loop above should process all chains.
            for (RegNum dst = 0; dst < regCount; dst++) {
                if (state[dst] == DONE)
                    continue;
                ASSERT(nodes[dst].Src() != temp || dstLoc(dst) == temp);
                // The loop will be optimized out.
            }

            // A cycle is present and there are no chains left.
            // So, any non-done `dst` is part of a cycle.
            for (RegNum dst = 0; dst < regCount; dst++) {
                if (state[dst] == DONE)
                    continue;
                auto src = nodes[dst].Src();
                assign(tempIdx, src);
                nodes[dst].SetSrc(temp);
                changed = true;
                break;
            }
        }

        for (RegNum dst = 0; dst < regCount; dst++) {
            ASSERT(state[dst] == DONE);
            // The loop will be optimized out.
        }
    };

    auto assignIr = [&emitter, &iregNodes](RegNum dst, Location src) {
        auto& node = iregNodes[dst];
        if (node.isLoad) {
            // Loads always use loadInfo.src as the source.
            emitter.AssignIRegFromAddr(IReg::From(dst), node.loadInfo.src, node.loadInfo.offset, node.loadInfo.ldk);
        } else {
            emitter.AssignIReg(IReg::From(dst), IReg::From(src.IRegIdx()));
        }
    };

    auto assignFr = [&emitter, &fregNodes](RegNum dst, Location src) {
        auto& node = fregNodes[dst];
        if (node.isLoad) {
            // Loads always use loadInfo.src as the source.
            emitter.AssignFRegFromAddr(FReg::From(dst), node.loadInfo.src, node.loadInfo.offset, node.loadInfo.ldk);
        } else {
            emitter.AssignFReg(FReg::From(dst), FReg::From(src.FRegIdx()));
        }
    };

    // Order is important!
    // The freg graph is resolved first: loads in it read ireg addresses
    // that the ireg shuffle is not touched yet.
    cycleResolver(Utils::Span<AssignmentInfo>(fregNodes, FReg::COUNT), Location::FReg(TEMP_FR), assignFr);
    cycleResolver(Utils::Span<AssignmentInfo>(iregNodes, IReg::VIRT_COUNT), Location::IReg(tempIr), assignIr);

    // Handle (slot -> <any>) assignments.
    for (auto& assignment : assignments) {
        auto dst   = assignment.dst;
        auto src   = assignment.src;
        auto dkind = dst.Kind();

        if (assignment.fromAddr && src.Kind() == Location::SLOT) {
            EmitLoadFromAddr(emitter, dst, src, assignment.offset, assignment.ldk);
        } else if (src.Kind() == Location::SLOT && dkind == Location::IREG) {
            emitter.AssignIRegFromSlot(IReg::From(dst.IRegIdx()), src.SlotIdx());
        } else if (src.Kind() == Location::SLOT && dkind == Location::FREG) {
            emitter.AssignFRegFromSlot(FReg::From(dst.FRegIdx()), src.SlotIdx());
        }
    }
    return true;
}

} // namespace Cbc
