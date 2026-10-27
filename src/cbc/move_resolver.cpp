#include "move_resolver.h"
#include "cbc/isa.h"
#include "utils/assertion.h"
#include "utils/function.h"
#include "utils/span.h"
#include <cstdint>

namespace Cbc {

static constexpr IReg IREG_TEMP = PlatformTraits<HOST_PLATFORM>::TR;
static constexpr FReg FREG_TEMP = FReg::FR15;
static constexpr int REG_COUNT  = IReg::VIRT_COUNT + FReg::COUNT;

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

    bool isLoad { false };

    AssignmentInfo() : moveInfo {} {}

    AssignmentInfo(Location src) : moveInfo { src } {}

    AssignmentInfo(Location src, int32_t offset, Format::LoadAccessKind ldk)
        : loadInfo { src, offset, ldk },
          isLoad(true)
    {}

    Location Src() { return isLoad ? loadInfo.src : moveInfo.src; }

    static AssignmentInfo Move(Location src) { return AssignmentInfo(src); }

    static AssignmentInfo Load(Location src, int32_t offset, Format::LoadAccessKind ldk)
    {
        return AssignmentInfo(src, offset, ldk);
    }
};

static constexpr uint32_t Pack(enum Location::Kind src, enum Location::Kind dst, bool isLoad)
{
    constexpr auto kindSize = sizeof(enum Location::Kind);
    static_assert(kindSize == 1);
    static_assert(kindSize + kindSize + sizeof(bool) <= sizeof(uint32_t));
    return (static_cast<uint32_t>(src) << 16) | (static_cast<uint32_t>(dst) << 8) | static_cast<uint32_t>(isLoad);
}

constexpr bool LOAD = true;
constexpr bool MOVE = false;

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
    //       a cycle. Break it by executing one node's assignment into `temp`
    //       and rewriting the node to `dst = temp`. The `temp` register is
    //       guaranteed not to be in the cycle (by ABI constraints).
    // 3. When all nodes are DONE, the mapping is fully resolved.
    //
    // Complexity: O(n^2)
    // O(n) iterations in the worst case (one node resolved per iteration for a single cycle).
    // No heap allocations — only stack-allocated arrays.

    AssignmentInfo nodes[REG_COUNT] = {};

    for (Location dst = 0; dst < REG_COUNT; ++dst)
        nodes[dst] = AssignmentInfo::Move(dst);

    for (auto& assignment : assignments) {
        auto dst   = assignment.dst;
        auto src   = assignment.src;
        auto dkind = dst.Kind();
        auto skind = src.Kind();

        switch (Pack(skind, dkind, assignment.fromAddr)) {
            // Reg moves (must always hold: src.kind == dst.kind)
            case Pack(Location::IREG, Location::IREG, MOVE):
            case Pack(Location::FREG, Location::FREG, MOVE): nodes[dst] = AssignmentInfo::Move(src); break;

            // Loads (must always hold: src.kind = IREG)
            case Pack(Location::IREG, Location::IREG, LOAD):
            case Pack(Location::IREG, Location::FREG, LOAD):
                nodes[dst] = AssignmentInfo::Load(src, assignment.offset, assignment.ldk);
                break;

            // Param slots cannot form any chains: emit immediately, before
            // the register shuffle.
            case Pack(Location::IREG, Location::SLOT, MOVE):
                emitter.AssignSlotFromIReg(dst.SlotIdx(), IReg::From(src.IRegIdx()));
                break;
            case Pack(Location::FREG, Location::SLOT, MOVE):
                emitter.AssignSlotFromFReg(dst.SlotIdx(), FReg::From(src.FRegIdx()));
                break;
            case Pack(Location::SLOT, Location::SLOT, MOVE):
                emitter.AssignParamSlot(dst.SlotIdx(), src.SlotIdx());
                break;

            case Pack(Location::IREG, Location::SLOT, LOAD):
            case Pack(Location::SLOT, Location::SLOT, LOAD):
                EmitLoadFromAddr(emitter, dst, src, assignment.offset, assignment.ldk);
                break;

            // Untyped slots are immutable: processed after the register shuffle.
            case Pack(Location::SLOT, Location::IREG, MOVE):
            case Pack(Location::SLOT, Location::FREG, MOVE):

            case Pack(Location::SLOT, Location::IREG, LOAD):
            case Pack(Location::SLOT, Location::FREG, LOAD): break;

            default:
                FATAL(
                    "unexpected assignment: %c <- %c (load: %s)",
                    static_cast<int>(dkind),
                    static_cast<int>(skind),
                    assignment.fromAddr ? "true" : "false"
                );
        }
    }

    auto cycleResolver = [](Utils::Span<AssignmentInfo> nodes,
                            Utils::Function<void(Location, AssignmentInfo)> const& assign) {
        ASSERT(nodes.Size() == REG_COUNT);
        // The graph is: "nodes[dst].Src() -> dst"

        constexpr char NOT_VISITED = 0;
        constexpr char DONE        = 1;
        char state[REG_COUNT]      = { NOT_VISITED };

        for (Location dst = 0; dst < REG_COUNT; ++dst) {
            if (nodes[dst].Src() == dst && !nodes[dst].isLoad)
                state[dst] = DONE;
        }

        bool changed = true; // fixed-point iteration
        while (changed) {
            changed = false;
            // Find and resolve chains.
            for (Location dst = 0; dst < REG_COUNT; ++dst) {
                if (state[dst] == DONE) {
                    continue;
                }
                auto src = nodes[dst].Src();
                ASSERT(src != dst || nodes[dst].isLoad);

                // check if someone needs a value of `dst`.
                bool needed = false;
                for (Location other = 0; other < REG_COUNT; ++other) {
                    if (state[other] == DONE || other == dst)
                        continue;
                    if (nodes[other].Src() == dst) {
                        needed = true;
                        break;
                    }
                }
                if (needed)
                    continue;
                // safe to assign src to dst
                assign(dst, nodes[dst]);
                changed    = true;
                state[dst] = DONE;
            }
            if (changed)
                continue;

            Location temps[] = { Location::IReg(IREG_TEMP), Location::FReg(FREG_TEMP) };

            // Invariant: any in-progress assignment cannot have a `temp` register.
            // 1. A `temp` register cannot be part of a cycle (due to the ABI).
            // 2. The loop above should process all chains.
            for (Location temp : temps) {
                ASSERT(state[temp] == DONE);
                for (Location dst = 0; dst < REG_COUNT; ++dst) {
                    if (state[dst] == DONE)
                        continue;
                    ASSERT(nodes[dst].Src() != temp || dst == temp);
                    // The loop will be optimized out.
                }
            }

            // A cycle is present and there are no chains left.
            // So, any non-done `dst` is part of a cycle.
            for (Location dst = 0; dst < REG_COUNT; ++dst) {
                if (state[dst] == DONE)
                    continue;
                Location tmp = dst.Kind() == Location::IREG ? temps[0] : temps[1];
                assign(tmp, nodes[dst]);
                nodes[dst] = AssignmentInfo::Move(tmp);
                changed = true;
                break;
            }
        }

        for (Location dst = 0; dst < REG_COUNT; ++dst) {
            ASSERT(state[dst] == DONE);
            // The loop will be optimized out.
        }
    };

    auto assign = [&emitter](Location dst, AssignmentInfo info) {
        if (info.isLoad) {
            Location src = info.loadInfo.src;
            EmitLoadFromAddr(emitter, dst, src, info.loadInfo.offset, info.loadInfo.ldk);
        } else {
            Location src = info.moveInfo.src;
            switch (dst.Kind()) {
                case Location::IREG: emitter.AssignIReg(IReg::From(dst.IRegIdx()), IReg::From(src.IRegIdx())); break;
                case Location::FREG: emitter.AssignFReg(FReg::From(dst.FRegIdx()), FReg::From(src.FRegIdx())); break;
                default:             ASSERT(false);
            }
        }
    };

    cycleResolver(Utils::Span<AssignmentInfo>(nodes, REG_COUNT), assign);

    // Handle (slot -> <any>) assignments.
    for (auto& assignment : assignments) {
        auto dst   = assignment.dst;
        auto src   = assignment.src;
        auto dkind = dst.Kind();

        if (assignment.fromAddr && src.Kind() == Location::SLOT && dkind != Location::SLOT) {
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
