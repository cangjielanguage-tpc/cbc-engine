#pragma once

#include <ostream>

#include "cbc/isa.h"
#include "cbc/move_resolver.h"
#include "utils/assertion.h"

namespace Cbc {

static constexpr int kAddend = 128;

static inline int IrValue(int ireg) { return ireg; }

static inline int FrValue(int freg) { return freg + kAddend; }

static inline int StValue(int slot) { return slot + 2 * kAddend; }

struct Cell {
    enum Tag : char {
        NONE,
        MOVE,
        LOAD
    };

    Tag tag;
    int value;                      // MOVE: the moved value; LOAD: the value in the address register
    int32_t offset             = 0; // LOAD: byte offset
    Format::LoadAccessKind ldk = Format::LoadAccessKind::LD_64; // LOAD: access kind

    static Cell None(int value = -1) { return { NONE, value }; }

    static Cell Move(int value) { return { MOVE, value }; }

    static Cell Load(int addrValue, int32_t offset, Format::LoadAccessKind ldk)
    {
        return { LOAD, addrValue, offset, ldk };
    }

    bool operator==(const Cell& other) const
    {
        return tag == other.tag && value == other.value && offset == other.offset && ldk == other.ldk;
    }

    bool operator!=(const Cell& other) const { return !(*this == other); }
};

inline std::ostream& operator<<(std::ostream& os, const Cell& cell)
{
    const char* tag = cell.tag == Cell::NONE ? "None" : cell.tag == Cell::MOVE ? "Move" : "Load";
    return os << tag << "(" << cell.value << ", " << cell.offset << ", " << cell.ldk.CStr() << ")";
}

struct RegsAndSlots : public MoveEmitter {
    static constexpr int stackSlotCount = kAddend;

    Cell iregs[IReg::COUNT];
    Cell fregs[FReg::COUNT];
    Cell untyped[stackSlotCount];

    uint8_t SlotSizes[stackSlotCount] {};
    // Byte-addressed view of the passing area: compacted arguments share a
    // slot, so each byte offset gets its own cell.
    Cell paramBytes[stackSlotCount * 8] {};

    int movCount = 0;

    RegsAndSlots()
    {
        for (int i = 0; i < IReg::COUNT; i++)
            iregs[i] = Cell::None(IrValue(i));
        for (int i = 0; i < FReg::COUNT; i++)
            fregs[i] = Cell::None(FrValue(i));
        for (int i = 0; i < stackSlotCount; i++)
            untyped[i] = Cell::None(StValue(i));
    }

    /// Value of the slot-aligned argument in `slot` (byte offset 0).
    Cell& Slot(int n) { return ParamAt(n, 0); }

    const Cell& Slot(int n) const { return ParamAt(n, 0); }

    /// Records a stack argument at `slot * 8 + byteOffset`.
    static size_t slotIdx(int slot, uint32_t byteOffset) { return static_cast<size_t>(slot) * 8 + byteOffset; }

    Cell& ParamAt(int slot, uint32_t byteOffset) { return paramBytes[slotIdx(slot, byteOffset)]; }

    const Cell& ParamAt(int slot, uint32_t byteOffset) const { return paramBytes[slotIdx(slot, byteOffset)]; }

    Cell Read(Location src) const
    {
        switch (src.Kind()) {
            case Location::IREG: return iregs[src.IRegIdx()];
            case Location::FREG: return fregs[src.FRegIdx()];
            case Location::SLOT: return untyped[src.SlotIdx()];
            case Location::NIL: FATAL("illegal src nil");
        }
    }

    void Assign(Location dst, Cell cell)
    {
        switch (dst.Kind()) {
            case Location::IREG: iregs[dst.IRegIdx()] = cell; break;
            case Location::FREG: fregs[dst.FRegIdx()] = cell; break;
            case Location::SLOT: ParamAt(dst.SlotIdx(), 0) = cell; break;
            case Location::NIL: FATAL("illegal dst nil");
        }
        movCount++;
    }

    /// Value moved to a destination: a pristine source becomes a MOVE cell.
    Cell Moved(Location src)
    {
        Cell cell = Read(src);
        if (cell.tag == Cell::NONE)
            cell.tag = Cell::MOVE;
        return cell;
    }

    void Mov(Location dst, Location src)
    {
        // A move stamps a pristine value as moved; a load record keeps its provenance.
        Cell cell = Read(src);
        if (cell.tag == Cell::NONE)
            cell.tag = Cell::MOVE;
        Assign(dst, cell);
    }

    void LoadTo(Location dst, Location addr, int32_t offset, Format::LoadAccessKind ldk)
    {
        Assign(dst, Cell::Load(Read(addr).value, offset, ldk));
    }

    void AssignIReg(IReg dst, IReg src) override { Mov(Location::IReg(dst), Location::IReg(src)); }

    void AssignFReg(FReg dst, FReg src) override { Mov(Location::FReg(dst), Location::FReg(src)); }

    void AssignParamSlot(int paramSlot, int stackSlot, Format::StoreAccessKind stk, uint32_t byteOffset) override
    {
        SlotSizes[slotIdx(paramSlot, byteOffset)] = static_cast<uint8_t>(stk);
        ParamAt(paramSlot, byteOffset)            = Moved(Location::Slot(stackSlot));
        movCount++;
    }

    void AssignFRegFromSlot(FReg dst, int slot) override { Mov(Location::FReg(dst), Location::Slot(slot)); }

    void AssignIRegFromSlot(IReg dst, int slot) override { Mov(Location::IReg(dst), Location::Slot(slot)); }

    void AssignSlotFromIReg(int slot, IReg src, Format::StoreAccessKind stk, uint32_t byteOffset) override
    {
        SlotSizes[slotIdx(slot, byteOffset)] = static_cast<uint8_t>(stk);
        ParamAt(slot, byteOffset)            = Moved(Location::IReg(src));
        movCount++;
    }

    void AssignSlotFromFReg(int slot, FReg src, Format::StoreAccessKind stk, uint32_t byteOffset) override
    {
        SlotSizes[slotIdx(slot, byteOffset)] = static_cast<uint8_t>(stk);
        ParamAt(slot, byteOffset)            = Moved(Location::FReg(src));
        movCount++;
    }

    void AssignIRegFromAddr(IReg dst, Location addr, int32_t offset, Format::LoadAccessKind ldk) override
    {
        LoadTo(Location::IReg(dst), addr, offset, ldk);
    }

    void AssignFRegFromAddr(FReg dst, Location addr, int32_t offset, Format::LoadAccessKind ldk) override
    {
        LoadTo(Location::FReg(dst), addr, offset, ldk);
    }

    void AssignSlotFromAddr(
        int paramSlot, Location addr, int32_t offset, Format::LoadAccessKind ldk, uint32_t byteOffset
    ) override
    {
        LoadTo(Location::Slot(paramSlot), addr, offset, ldk);
    }

    void LeaParamArea(IReg dst, uint32_t offset) override
    {
        Assign(
            Location::IReg(dst),
            Cell::Load(Read(Location::IReg(dst)).value, static_cast<int32_t>(offset), Format::LoadAccessKind::LD_LEA)
        );
    }

    void Resolve(MoveResolver& mr) { mr.Resolve(*this); }
};

} // namespace Cbc
