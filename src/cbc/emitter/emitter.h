#pragma once

#include <vector>
#include <functional>
#include <cstdint>
#include <memory_resource>
#include <memory>

#include "utils/assertion.h"
#include "cbc/isa.h"
#include "cbc/emitter/symbols.h"
#include "cbc/emitter/segment.h"
#include "interpreter/code.h"
#include "encoding_rt.h"

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
    using Width = Format::Width;
    using CC = Format::CC;
    using Common = Format::Common;
    using Bits = Format::Bits;

    class MemSpace {
    public:
        MemSpace(Emitter& _emitter)
            : segment(_emitter.segment), symbols(_emitter.symbols), emitter(_emitter) {}

        void Offset(uint64_t offset);
        void OffsetReg(IReg reg);

        // tail instructions
        void LoadObj (Format::LoadAccessKind  ldk, Format::Reg dst, IReg base);
        void StoreObj(Format::StoreAccessKind stk, Format::Reg src, IReg base);

        void LoadRec (Format::LoadAccessKind  ldk, Format::Reg dst, IReg base);
        void StoreRec(Format::StoreAccessKind stk, Format::Reg src, IReg base);

    private:

        template <typename AccessKind>
        void LoadStore(AccessKind akind, Format::Reg v, IReg base, RT::MemOpcode opc)
        {
            Encode(segment, RT::M2rr {
                .opc = opc,
                .rr = Format::RR {
                    .x = v,
                    .y = base
                },
            });
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

    void Binary(Format::Common op, Format::Width width, IReg d, IReg l, IReg r);
    void Add (Width width, IReg d, IReg l, IReg r);
    void Sub (Width width, IReg d, IReg l, IReg r);
    void Mul (Width width, IReg d, IReg l, IReg r);
    void And (Width width, IReg d, IReg l, IReg r);
    void Or  (Width width, IReg d, IReg l, IReg r);
    void Xor (Width width, IReg d, IReg l, IReg r);
    void Div (Width width, IReg d, IReg l, IReg r);
    void Rem (Width width, IReg d, IReg l, IReg r);
    void UDiv(Width width, IReg d, IReg l, IReg r);
    void URem(Width width, IReg d, IReg l, IReg r);
    void Lsl (Width width, IReg d, IReg l, IReg r);
    void Lsr (Width width, IReg d, IReg l, IReg r);
    void Asr (Width width, IReg d, IReg l, IReg r);

    void BinaryImm(Format::Common op, Format::Width width, IReg d, IReg l, uint64_t imm);
    void AddI (Width width, IReg d, IReg l, uint64_t imm);
    void SubI (Width width, IReg d, IReg l, uint64_t imm);
    void MulI (Width width, IReg d, IReg l, uint64_t imm);
    void AndI (Width width, IReg d, IReg l, uint64_t imm);
    void OrI  (Width width, IReg d, IReg l, uint64_t imm);
    void XorI (Width width, IReg d, IReg l, uint64_t imm);
    void DivI (Width width, IReg d, IReg l, uint64_t imm);
    void RemI (Width width, IReg d, IReg l, uint64_t imm);
    void UDivI(Width width, IReg d, IReg l, uint64_t imm);
    void URemI(Width width, IReg d, IReg l, uint64_t imm);
    void LslI (Width width, IReg d, IReg l, uint64_t imm);
    void LsrI (Width width, IReg d, IReg l, uint64_t imm);
    void AsrI (Width width, IReg d, IReg l, uint64_t imm);

    void Add(Width width, FReg d, FReg l, FReg r);
    void Sub(Width width, FReg d, FReg l, FReg r);
    void Mul(Width width, FReg d, FReg l, FReg r);
    void Div(Width width, FReg d, FReg l, FReg r);

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
    void LoadObj (Format::LoadAccessKind  ldk, Format::Reg dst, IReg base, uint32_t offset);
    void StoreObj(Format::StoreAccessKind stk, Format::Reg src, IReg base, uint32_t offset);

    void LoadRec (Format::LoadAccessKind  ldk, Format::Reg dst, IReg base, uint32_t offset);
    void StoreRec(Format::StoreAccessKind stk, Format::Reg src, IReg base, uint32_t offset);

    MemSpace OpenMemSpace();

private:
    void AddFixup(std::unique_ptr<Fixup> fixup);
    void Mov(RT::Opcode opcode, Format::Reg d, Format::Reg s);
    void Binary(Format::FloatOperations op, Format::Width width, FReg d, FReg l, FReg r);

    template <typename AccessKind>
    void LoadStore(AccessKind akind, Format::Reg v, IReg base, uint32_t offset, RT::Opcode opc)
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
