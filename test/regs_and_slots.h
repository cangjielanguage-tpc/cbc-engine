#pragma once

#include <ostream>

#include "cbc/frame.h"
#include "cbc/isa.h"
#include "cbc/move_resolver.h"
#include "utils/assertion.h"

namespace Cbc {

static constexpr int kAddend = 128;

static inline int IrValue(int ireg) { return ireg; }

static inline int FrValue(int freg) { return freg + kAddend; }

static inline int StValue(int slot) { return slot + 2 * kAddend; }

// Symbolic contents of a test register or stack location. Tracks value source
// rather than actual numeric data, so tests can detect clobbered or misplaced arguments.
// Moving a LOAD or ADDRESS cell preserves its tag and source.
struct Cell {
    enum Tag : char {
        NONE,   // Untouched location: value identifies its original contents; -1 means empty.
        MOVE,   // Copied original contents: value identifies the source, not its current location.
        LOAD,   // Symbolic memory read: *(source identified by value + offset), using ldk.
        ADDRESS // Pointer into the outgoing passing area: offset is relative to its base; value is unused.
    };

    Tag tag;
    int value;
    int32_t offset             = 0;
    Format::LoadAccessKind ldk = Format::LoadAccessKind::LD_64;

    static Cell None(int value = -1) { return { NONE, value }; }

    static Cell Move(int value) { return { MOVE, value }; }

    static Cell Load(int addrValue, int32_t offset, Format::LoadAccessKind ldk)
    {
        return { LOAD, addrValue, offset, ldk };
    }

    static Cell ParamAddress(uint32_t offset)
    {
        return { ADDRESS, 0, static_cast<int32_t>(offset), Format::LoadAccessKind::LD_LEA };
    }

    bool operator==(const Cell& other) const
    {
        return tag == other.tag && value == other.value && offset == other.offset && ldk == other.ldk;
    }

    bool operator!=(const Cell& other) const { return !(*this == other); }
};

inline std::ostream& operator<<(std::ostream& os, const Cell& cell)
{
    const char* tag = cell.tag == Cell::NONE   ? "None"
                      : cell.tag == Cell::MOVE ? "Move"
                      : cell.tag == Cell::LOAD ? "Load"
                                               : "Address";
    return os << tag << "(" << cell.value << ", " << cell.offset << ", " << cell.ldk.CStr() << ")";
}

struct RegsAndSlots : public MoveEmitter {
    static constexpr int stackSlotCount = kAddend;
    static constexpr int stackByteCount = stackSlotCount * sizeof(uint64_t);

    Cell iregs[IReg::COUNT];
    Cell fregs[FReg::COUNT];
    Cell untyped[stackSlotCount];

    uint8_t storeKinds[stackByteCount] {};
    Cell paramBytes[stackByteCount] {};
    bool writtenBytes[stackByteCount] {};
    bool overlappingWrites = false;

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

    static size_t ParamIndex(int slot, uint32_t byteOffset)
    {
        ASSERT(slot >= 0 && slot < stackSlotCount);
        ASSERT(byteOffset < STACK_SLOT_SIZE);
        return static_cast<size_t>(slot) * STACK_SLOT_SIZE + byteOffset;
    }

    Cell& ParamAt(int slot, uint32_t byteOffset) { return paramBytes[ParamIndex(slot, byteOffset)]; }

    const Cell& ParamAt(int slot, uint32_t byteOffset) const { return paramBytes[ParamIndex(slot, byteOffset)]; }

    void WriteParam(int slot, uint32_t byteOffset, Format::StoreAccessKind stk, Cell cell)
    {
        size_t width;
        switch (stk) {
            case Format::StoreAccessKind::ST_8:   width = 1; break;
            case Format::StoreAccessKind::ST_16:
            case Format::StoreAccessKind::ST_F16: width = 2; break;
            case Format::StoreAccessKind::ST_32:
            case Format::StoreAccessKind::ST_F32: width = 4; break;
            case Format::StoreAccessKind::ST_64:
            case Format::StoreAccessKind::ST_F64:
            case Format::StoreAccessKind::ST_REF: width = 8; break;
            default:                              FATAL("unexpected stack store kind");
        }
        auto index = ParamIndex(slot, byteOffset);
        ASSERT(index + width <= stackByteCount);
        for (size_t i = index; i < index + width; i++) {
            overlappingWrites |= writtenBytes[i];
            writtenBytes[i]    = true;
        }
        storeKinds[index] = static_cast<uint8_t>(stk);
        paramBytes[index] = cell;
        movCount++;
    }

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

    Cell Moved(Location src)
    {
        Cell cell = Read(src);
        if (cell.tag == Cell::NONE)
            cell.tag = Cell::MOVE;
        return cell;
    }

    void Mov(Location dst, Location src) { Assign(dst, Moved(src)); }

    void LoadTo(Location dst, Location addr, int32_t offset, Format::LoadAccessKind ldk)
    {
        auto source = Read(addr);
        if (source.tag == Cell::ADDRESS) {
            auto index = source.offset + offset;
            ASSERT(index >= 0 && index < stackByteCount);
            ASSERT(writtenBytes[index]);
            Assign(dst, paramBytes[index]);
        } else {
            Assign(dst, Cell::Load(source.value, offset, ldk));
        }
    }

    void AssignIReg(IReg dst, IReg src) override { Mov(Location::IReg(dst), Location::IReg(src)); }

    void AssignFReg(FReg dst, FReg src) override { Mov(Location::FReg(dst), Location::FReg(src)); }

    void AssignParamSlot(int paramSlot, int stackSlot, Format::StoreAccessKind stk, uint32_t byteOffset) override
    {
        WriteParam(paramSlot, byteOffset, stk, Moved(Location::Slot(stackSlot)));
    }

    void AssignFRegFromSlot(FReg dst, int slot) override { Mov(Location::FReg(dst), Location::Slot(slot)); }

    void AssignIRegFromSlot(IReg dst, int slot) override { Mov(Location::IReg(dst), Location::Slot(slot)); }

    void AssignSlotFromIReg(int slot, IReg src, Format::StoreAccessKind stk, uint32_t byteOffset) override
    {
        WriteParam(slot, byteOffset, stk, Moved(Location::IReg(src)));
    }

    void AssignSlotFromFReg(int slot, FReg src, Format::StoreAccessKind stk, uint32_t byteOffset) override
    {
        WriteParam(slot, byteOffset, stk, Moved(Location::FReg(src)));
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
        WriteParam(paramSlot, byteOffset, StkOfLdk(ldk), Cell::Load(Read(addr).value, offset, ldk));
    }

    void LeaParamArea(IReg dst, uint32_t offset) override { Assign(Location::IReg(dst), Cell::ParamAddress(offset)); }

    void Resolve(MoveResolver& mr) { mr.Resolve(*this); }
};

} // namespace Cbc
