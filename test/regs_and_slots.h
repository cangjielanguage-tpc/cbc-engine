#pragma once

#include "cbc/isa.h"
#include "cbc/move_resolver.h"
#include "utils/assertion.h"

namespace Cbc {

static constexpr int kAddend = 128;

static inline int IrValue(int ireg) { return ireg; }

static inline int FrValue(int freg) { return freg + kAddend; }

static inline int StValue(int slot) { return slot + 2 * kAddend; }

struct RegsAndSlots {
    static constexpr int stackSlotCount = kAddend;

    int iregs[IReg::VIRT_COUNT];
    int fregs[FReg::COUNT];
    int untyped[stackSlotCount];
    int paramPassingStackSlots[stackSlotCount];

    int movCount = 0;

    RegsAndSlots()
    {
        for (int i = 0; i < IReg::VIRT_COUNT; i++)
            iregs[i] = IrValue(i);
        for (int i = 0; i < FReg::COUNT; i++)
            fregs[i] = FrValue(i);
        for (int i = 0; i < stackSlotCount; i++)
            untyped[i] = StValue(i);
        for (int i = 0; i < stackSlotCount; i++)
            paramPassingStackSlots[i] = -1;
    }

    void Mov(Location dst, Location src)
    {
        int value;
        int sidx, didx;

        switch (src.Kind()) {
            case Location::IREG:
                sidx  = src.IRegIdx();
                value = iregs[sidx];
                break;
            case Location::FREG:
                sidx  = src.FRegIdx();
                value = fregs[sidx];
                break;
            case Location::SLOT:
                sidx  = src.SlotIdx();
                value = untyped[sidx];
                break;
            case Location::NIL: FATAL("illegal src nil");
        }

        switch (dst.Kind()) {
            case Location::IREG:
                didx        = dst.IRegIdx();
                iregs[didx] = value;
                break;
            case Location::FREG:
                didx        = dst.FRegIdx();
                fregs[didx] = value;
                break;
            case Location::SLOT:
                didx                         = dst.SlotIdx();
                paramPassingStackSlots[didx] = value;
                break;
            case Location::NIL: FATAL("illegal dst nil");
        }
        movCount++;
    }

    void Resolve(MoveResolver& mr)
    {
        auto emit = [this](Location dst, Location src) { this->Mov(dst, src); };
        mr.Resolve(emit);
    }
};

} // namespace Cbc
