#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <memory_resource>
#include <vector>

#include "cbc/emitter/segment.h"
#include "cbc/emitter/symbols.h"
#include "cbc/isa.h"
#include "encoding_rt.h"
#include "interpreter/code.h"
#include "interpreter/function_handle.h"
#include "utils/assertion.h"

namespace Cbc {
namespace Emitter {

class CbcEmitter;
class Symbols;

struct EmitterSnapshot {
    SegmentSnapshot segmentSnapshot;
    size_t fixupCount;
};

class Emitter {
public:
    using Reg    = Format::Reg;
    using Width  = Format::Width;
    using CC     = Format::CC;
    using Common = Format::Common;
    using Bits   = Format::Bits;

    using FloatOperations = Format::FloatOperations;
    using LoadAccessKind  = Format::LoadAccessKind;
    using StoreAccessKind = Format::StoreAccessKind;

    class MemSpace {
    public:
        MemSpace(Emitter& _emitter) : segment(_emitter.segment), symbols(_emitter.symbols), emitter(_emitter) {}

        void Offset(uint64_t offset);
        void OffsetReg(IReg reg);

        // tail instructions
        void LoadObj(LoadAccessKind ldk, Reg dst, IReg base);
        void StoreObj(StoreAccessKind stk, Reg src, IReg base);

        void LoadRec(LoadAccessKind ldk, Reg dst, IReg base);
        void StoreRec(StoreAccessKind stk, Reg src, IReg base);

        void LoadFrame(LoadAccessKind ldk, Reg dst);
        void StoreFrame(StoreAccessKind stk, Reg src);

    private:
        template <typename AccessKind> void LoadStore(AccessKind akind, Reg v, IReg base, RT::MemOpcode opc)
        {
            Encode(
                segment,
                RT::M2rr {
                    .opc = opc,
                    .rr  = Format::RR { .x = v, .y = base },
                }
            );
        }

        Segment& segment;
        Symbols& symbols;
        Emitter& emitter;
    };

    Emitter() = default;

    Symbol NewAddressSym(uintptr_t ptr);
    Label NewLabel();
    void Bind(Label label);
    // TODO: add symbol kind to store arbitrary-size values.

    /// Build `Code` in given `heap`.
    ///
    /// This procedure resolves all existring fixups and
    /// creates literal table (unused symbols or fixups will be discarded).
    Interpretation::Code Build(std::pmr::memory_resource& heap);

    EmitterSnapshot Snapshot();
    void Apply(EmitterSnapshot snapshot);

    void Binary(Common op, Width width, IReg d, IReg l, IReg r);
    void Add(Width width, IReg d, IReg l, IReg r);
    void Sub(Width width, IReg d, IReg l, IReg r);
    void Mul(Width width, IReg d, IReg l, IReg r);
    void And(Width width, IReg d, IReg l, IReg r);
    void Or(Width width, IReg d, IReg l, IReg r);
    void Xor(Width width, IReg d, IReg l, IReg r);
    void Div(Width width, IReg d, IReg l, IReg r);
    void Rem(Width width, IReg d, IReg l, IReg r);
    void UDiv(Width width, IReg d, IReg l, IReg r);
    void URem(Width width, IReg d, IReg l, IReg r);
    void Lsl(Width width, IReg d, IReg l, IReg r);
    void Lsr(Width width, IReg d, IReg l, IReg r);
    void Asr(Width width, IReg d, IReg l, IReg r);
    void Neg(Width width, IReg d, IReg s);

    void BinaryImm(Common op, Width width, IReg d, IReg l, uint64_t imm);
    void AddI(Width width, IReg d, IReg l, uint64_t imm);
    void SubI(Width width, IReg d, IReg l, uint64_t imm);
    void MulI(Width width, IReg d, IReg l, uint64_t imm);
    void AndI(Width width, IReg d, IReg l, uint64_t imm);
    void OrI(Width width, IReg d, IReg l, uint64_t imm);
    void XorI(Width width, IReg d, IReg l, uint64_t imm);
    void DivI(Width width, IReg d, IReg l, uint64_t imm);
    void RemI(Width width, IReg d, IReg l, uint64_t imm);
    void UDivI(Width width, IReg d, IReg l, uint64_t imm);
    void URemI(Width width, IReg d, IReg l, uint64_t imm);
    void LslI(Width width, IReg d, IReg l, uint64_t imm);
    void LsrI(Width width, IReg d, IReg l, uint64_t imm);
    void AsrI(Width width, IReg d, IReg l, uint64_t imm);

    void Binary(FloatOperations op, Width width, FReg d, FReg l, FReg r);
    void Add(Width width, FReg d, FReg l, FReg r);
    void Sub(Width width, FReg d, FReg l, FReg r);
    void Mul(Width width, FReg d, FReg l, FReg r);
    void Div(Width width, FReg d, FReg l, FReg r);

    void Unary(FloatOperations op, Width Width, FReg d, FReg s);
    void Sqrt(Width width, FReg d, FReg s);
    void Abs(Width width, FReg d, FReg s);
    void Neg(Width width, FReg d, FReg s);

    void Ret();
    void Mov(IReg d, IReg s);
    void Mov(FReg d, FReg s);
    void Mov(IReg d, FReg s);
    void Mov(FReg d, IReg s);
    void MovImm(Width width, IReg d, uint64_t imm);
    void FMovI32(FReg d, float imm);
    void FMovI64(FReg d, double imm);
    void MovRef(IReg d, IReg s);

    void Bcc(CC cc, Width width, IReg l, IReg r, Label label);
    void BccImm(CC cc, Width width, IReg l, uint64_t r, Label label);
    void Jmp(Label label);

    void NewObj(IReg d, Symbol sym);
    void LoadObj(LoadAccessKind ldk, Reg dst, IReg base, uint32_t offset);
    void StoreObj(StoreAccessKind stk, Reg src, IReg base, uint32_t offset);
    void LoadRec(LoadAccessKind ldk, Reg dst, IReg base, uint32_t offset);
    void StoreRec(StoreAccessKind stk, Reg src, IReg base, uint32_t offset);

    void LoadFrame(LoadAccessKind ldk, Reg dst, uint32_t offset);
    void StoreFrame(StoreAccessKind stk, Reg src, uint32_t offset);

    void SCC(CC cc, Width width, IReg d, IReg l, IReg r);
    void SCC(CC cc, Width width, IReg d, FReg l, FReg r);
    void SCCImm(CC cc, Width width, IReg d, IReg l, uint64_t imm);

    void DirectCall(IReg d, Symbol fuh);

    MemSpace OpenMemSpace();

private:
    void AddFixup(std::unique_ptr<Fixup> fixup);
    void Mov(RT::Opcode opcode, Reg d, Reg s);

    template <typename AccessKind> void LoadStore(AccessKind akind, Reg v, IReg base, uint32_t offset, RT::Opcode opc)
    {
        Encode(segment, RT::B4xi12rr {
            .opc = opc,
            .xi12 = {
                .imm4 = Format::Imm4(akind),
                .imm12 = static_cast<uint16_t>(offset),
            },
            .rr = {
                .x = v,
                .y = base,
            }
        });
    }

    Symbols symbols;
    Segment segment;

    std::vector<std::unique_ptr<Fixup>> fixups;
};

} // namespace Emitter
} // namespace Cbc
