#include <utility>
#include <cstring>

#include "emitter.h"

namespace Cbc {
namespace Emitter {

using Width = Format::Width;
using CC = Format::CC;
using Common = Format::Common;

Symbol Emitter::Address(uintptr_t ptr) {
    return symbols.Address(ptr);
}

Symbol Emitter::NewLabel() {
    return symbols.NewLabel();
}

void Emitter::Bind(Symbol label) {
    symbols.Bind(label, segment.Position());
}

EmitterSnapshot Emitter::Snapshot() {
    return EmitterSnapshot {
        .segmentSnapshot = segment.Snapshot(),
        .addressFixupsCount = addressFixups.size(),
        .jumpFixupsCount = jumpFixups.size()
    };
}

void Emitter::Apply(EmitterSnapshot snapshot) {
    segment.Apply(snapshot.segmentSnapshot);
    addressFixups.resize(snapshot.addressFixupsCount);
    jumpFixups.resize(snapshot.jumpFixupsCount);
}

Code Emitter::Build(std::pmr::memory_resource& heap) {
    auto segment = this->segment.Finish();
    auto addressFixups = std::exchange(this->addressFixups, {});
    auto jumpFixups = std::exchange(this->jumpFixups, {});

    auto bytecode = (uint8_t*) heap.allocate(segment.size());
    auto bytecodeSize = segment.size();
    std::memcpy(bytecode, &segment[0], bytecodeSize);

    // TODO: apply fixups

    return Code {
        .bytecodeSize = bytecodeSize,
    };
}

// region isa12

using namespace Format;

Bits pack8(Bits low4, Bits high4) {
    return high4.In(4).Shift(4) | low4.In(4);
}

Bits pack8(IReg r1, IReg r2) {
    return pack8(r1, r2);
}

Bits pack8(Bits v1, IReg r2) {
    return pack8(v1, r2);
}

Bits pack8(IReg r1, Bits v2) {
    return pack8(r1, v2);
}

Emitter::B3xrr_parts Emitter::PrepareBitsForB3Formats(Common op, Width width) {
    return PrepareBitsForB3Formats(op, width.Common());
}

Emitter::B3xrr_parts Emitter::PrepareBitsForB3Formats(Common op, Bits b1) {
    auto page = Bits(op >> 3);
    return B3xrr_parts {
        .low3BitsOfFormatByte = Bits(OP7A::COMMON).In(2).Shift(1) | page.In(1),
        .low4BitsOfSecondByte = (Bits(op) & 0x7).Shift(1) | b1.In(1),
    };
}

void Emitter::Add (Width width, IReg d, IReg l, IReg r) { GenCommon(Common::ADD,  width, d, l, r); }
void Emitter::Sub (Width width, IReg d, IReg l, IReg r) { GenCommon(Common::SUB,  width, d, l, r); }
void Emitter::Mul (Width width, IReg d, IReg l, IReg r) { GenCommon(Common::MUL,  width, d, l, r); }
void Emitter::And (Width width, IReg d, IReg l, IReg r) { GenCommon(Common::AND,  width, d, l, r); }
void Emitter::Or  (Width width, IReg d, IReg l, IReg r) { GenCommon(Common::OR,   width, d, l, r); }
void Emitter::Xor (Width width, IReg d, IReg l, IReg r) { GenCommon(Common::XOR,  width, d, l, r); }
void Emitter::Div (Width width, IReg d, IReg l, IReg r) { GenCommon(Common::SDIV, width, d, l, r, true); }
void Emitter::Rem (Width width, IReg d, IReg l, IReg r) { GenCommon(Common::SREM, width, d, l, r, true); }
void Emitter::UDiv(Width width, IReg d, IReg l, IReg r) { GenCommon(Common::UDIV, width, d, l, r); }
void Emitter::URem(Width width, IReg d, IReg l, IReg r) { GenCommon(Common::UREM, width, d, l, r); }
void Emitter::Lsl (Width width, IReg d, IReg l, IReg r) { GenCommon(Common::LSL,  width, d, l, r); }
void Emitter::Lsr (Width width, IReg d, IReg l, IReg r) { GenCommon(Common::LSR,  width, d, l, r); }
void Emitter::Asr (Width width, IReg d, IReg l, IReg r) { GenCommon(Common::ASR,  width, d, l, r); }


void Emitter::GenCommon(Common op, Width width, IReg d, IReg l, IReg r, bool prohibitB2r) {
    if (d == l && op.B2rAllowed() && !prohibitB2r) {
        GenB2rr(d, r, op, width);
    } else {
        GenB3xrrr(d, l, r, PrepareBitsForB3Formats(op, width));
    }
}

void Emitter::GenB2rr(IReg d, IReg r, Common common, Width width) {
    segment.AddW8(Format::B2rr::Fmt(common, width).Raw());
    segment.AddW8(pack8(d, r).Raw());
}

void Emitter::GenB3xrrr(IReg d, IReg l, IReg r, B3xrr_parts parts) {
    segment.AddW8(Format::B3xrrr::Fmt(parts.low3BitsOfFormatByte).Raw());
    segment.AddW8(pack8(parts.low4BitsOfSecondByte, d).Raw());
    segment.AddW8(pack8(l, r).Raw());
}

// endregion isa12

} // namespace Emitter
} // namespace Cbc
