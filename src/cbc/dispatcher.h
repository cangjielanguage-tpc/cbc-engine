#ifndef CBC_DISPATCHER_H
#define CBC_DISPATCHER_H

/// This file defines the infrastructure for mapping bytecode opcodes to their
/// corresponding handler functions. It utilizes compile-time table generation
/// to create a direct threaded code or function pointer table for efficient
/// instruction dispatch.
///
/// **Handler and Context:**
/// Both are used to pass VM state via registers. We split state into two
/// parameters to avoid the 16-byte limitation on a single parameter found
/// in many calling conventions, maximizing the data passed in registers.

#include "decoder.h"
#include "utils/assertion.h"
#include "cassert"

#if __has_attribute(musttail)
#define MUSTTAIL __attribute__((musttail))
#else
#error "__attribute__((musttail)) is required for correctness, please use a newer compiler"
#endif

#define NEXT MUSTTAIL return table<Handler>[stream.PeekOpcode()](handler, ctx, stream)
#define NEXT_COND(successful) MUSTTAIL return table<Handler>[successful ? stream.PeekOpcode() : 255](handler, ctx, stream)

namespace Decoder {
constexpr size_t TABLE_SIZE = 256;

template <typename Handler>
using TableFunction = void (*)(Handler handler, typename Handler::Context ctx, ByteReader stream);

template <typename Handler>
using Table = std::array<TableFunction<Handler>, TABLE_SIZE>;

using Width = Cbc::Format::Width;
using Sign = Cbc::Format::Sign;
using Common = Cbc::Format::Common;
using CC = Cbc::Format::CC;
using ImmKind = Cbc::Format::ImmKind;

template <typename Handler>
void Unreachable(Handler handler, typename Handler::Context ctx, ByteReader stream);

template <Width::Value width, typename Handler>
void B2rrMov(Handler handler, typename Handler::Context ctx, ByteReader stream);

template <typename Handler>
void B2rrMovRef(Handler handler, typename Handler::Context ctx, ByteReader stream);

template <typename Handler>
void B2rrMovVST(Handler handler, typename Handler::Context ctx, ByteReader stream);

template <Common::Value arithOp, Width::Value width, typename Handler>
void B2rrCommon(Handler handler, typename Handler::Context ctx, ByteReader stream);

template <Width::Value width, typename Handler>
void B2hrMovI(Handler handler, typename Handler::Context ctx, ByteReader stream);

template <CC::Value cc, Width::Value width, typename Handler>
void B2rrd8BranchIf(Handler handler, typename Handler::Context ctx, ByteReader stream);

template <typename Handler>
void B2xrIOpc1011SOC(Handler handler, typename Handler::Context ctx, ByteReader stream);

template <typename Handler>
bool B2xrIOpc1011SOCInternal(Handler handler, typename Handler::Context ctx, B2xrI args);

template <Cbc::Format::OP7A::Value op7a, Cbc::Format::Sign::Value sign, typename Handler>
void B3xrrrCommon(Handler handler, typename Handler::Context ctx, ByteReader stream);

template <Cbc::Format::OP7A::Value op7a, Cbc::Format::Sign::Value sign, typename Handler>
void B3xrrtiKCommon(Handler handler, typename Handler::Context ctx, ByteReader stream);

template <Cbc::Format::OP7A::Value op7a, Cbc::Format::Sign::Value sign, typename Handler>
bool B3xrrrOp7AInternal(Handler handler, typename Handler::Context ctx, B3xrrr args);

template <Cbc::Format::OP7A::Value op7a, Cbc::Format::Sign::Value sign, typename Handler>
bool B3xrrtiKOp7AInternal(Handler handler, typename Handler::Context ctx, B3xrrtiK args);

template <CC::Value cc, ImmKind::Value immKind, Width::Value width, typename Handler>
void ExtBcc(Handler handler, typename Handler::Context ctx, ByteReader stream);

template <typename Handler>
void ExtRet(Handler handler, typename Handler::Context ctx, ByteReader stream);

template <typename Enum, typename Enum::Value First, typename Enum::Value Last, typename F>
constexpr void ForRange(F f) {
    if constexpr (First <= Last) {
        f(std::integral_constant<typename Enum::Value, First>{});
        ForRange<Enum, typename Enum::Value(First + 1), Last>(f);
    }
}

template <auto... Vs, typename F>
constexpr void ForValues(F f) {
    (f(std::integral_constant<decltype(Vs), Vs>{}), ...);
}

template <typename Handler>
static constexpr auto table = [] {
    using namespace Cbc;

    Table<Handler> arr{};

    TableFunction<Handler> invalid = &Unreachable;

    for (auto &x : arr) {
        x = invalid;
    }

    auto table_put = [&](uint32_t location, TableFunction<Handler> func) {
        ASSERTION(arr[location] == invalid, "Location is already initialized");
        arr[location] = func;
    };

    table_put(Format::ExtRet::OPCODE, &ExtRet);

    // B2rr common operations
    ForRange<Width, Width::W32, Width::W64>([&](auto width) {
        ForRange<Common, Common::ADD, Common::XOR>([&](auto arithOp) {
            table_put(Format::B2rr::Fmt(arithOp, width), B2rrCommon<arithOp, width, Handler>);
        });
        table_put(Format::B2rr::Fmt(Common::MOV, width), B2rrMov<width, Handler>);
        table_put(Format::B2hr::Fmt(Common::MOV, width), B2hrMovI<width, Handler>);
    });

    table_put(Format::B2rr::Fmt(Common::MVST, Width::W32), B2rrMovVST);
    table_put(Format::B2rr::Fmt(Common::MREF, Width::W64), B2rrMovRef);

    // B3x common operations
    ForRange<Sign, Sign::SIGNED, Sign::UNSIGNED>([&](auto sign) {
        ForRange<Format::OP7A, Format::OP7A::COMMON, Format::OP7A::FLOAT>([&](auto op7a) {
            constexpr auto B3xLow3Bits = Format::Bits(op7a).In(2).Shift(1) | (Format::Bits(sign).In(1));
            table_put(Format::B3xrrr::Fmt(B3xLow3Bits).Raw(), B3xrrrCommon<op7a, sign, Handler>);
            table_put(Format::B3xrrt4i16::Fmt(B3xLow3Bits).Raw(), B3xrrtiKCommon<op7a, sign, Handler>);
        });
    });

    // B2rrd8 branch operations
    ForRange<Width, Width::W32, Width::W64>([&](auto width) {
        ForRange<CC, CC::EQ, CC::RNE>([&](auto cc) {
            table_put(Format::B2rrd8::Fmt(cc, width), B2rrd8BranchIf<cc, width, Handler>);
        });
    });

    // ExtBcc operations
    ForRange<Width, Width::W32, Width::W64>([&](auto width) {
        ForRange<ImmKind, ImmKind::VALUE, ImmKind::LITERAL>([&](auto immKind) {
            ForRange<CC, CC::EQ, CC::RNE>([&](auto cc) {
                table_put(Format::ExtBrr::Fmt(immKind, width, cc), ExtBcc<cc, immKind, width, Handler>);
            });
        });
    });

    // Symbolic opc1011
    table_put(Format::B2xrI::Opc1011::OPCODE.Raw(), B2xrIOpc1011SOC<Handler>);

    return arr;
}();

template <Width::Value width, typename Handler>
void B2rrMov(Handler handler, typename Handler::Context ctx, ByteReader stream) {
    auto args = B2rr::Decode(&stream);
    handler.StorePos(stream.Cursor());
    handler.template Mov<width>(ctx, args.Idst(), args.Isrc());
    NEXT;
}

template <Width::Value width, typename Handler>
void B2hrMovI(Handler handler, typename Handler::Context ctx, ByteReader stream) {
    auto args = B2hr::Decode(&stream);
    handler.StorePos(stream.Cursor());
    handler.template MovI<width>(ctx, args.Idst(), args.imm);
    NEXT;
}

template <typename Handler>
void B2rrMovRef(Handler handler, typename Handler::Context ctx, ByteReader stream) {
    auto args = B2rr::Decode(&stream);
    handler.StorePos(stream.Cursor());
    handler.MovRef(ctx, args.Idst(), args.Isrc());
    NEXT;
}

template <typename Handler>
void B2rrMovVST(Handler handler, typename Handler::Context ctx, ByteReader stream) {
    auto args = B2rr::Decode(&stream);
    handler.StorePos(stream.Cursor());
    handler.MovVST(ctx, args.Idst(), args.Isrc());
    NEXT;
}

template <Common::Value arithOp, Width::Value width, typename Handler>
void B2rrCommon(Handler handler, typename Handler::Context ctx, ByteReader stream) {
    auto args = B2rr::Decode(&stream);
    handler.StorePos(stream.Cursor());
    bool successful = handler.template Common2R<width, arithOp>(ctx, args.Idst(), args.Isrc());
    NEXT_COND(successful);
}

template <Cbc::Format::OP7A::Value op7a, Cbc::Format::Sign::Value sign, typename Handler>
void B3xrrrCommon(Handler handler, typename Handler::Context ctx, ByteReader stream) {
    auto args = B3xrrr::Decode(&stream);
    handler.StorePos(stream.Cursor());
    bool successful = B3xrrrOp7AInternal<op7a, sign>(handler, ctx, args);
    NEXT_COND(successful);
}

template <Cbc::Format::OP7A::Value op7a, Cbc::Format::Sign::Value sign, typename Handler>
void B3xrrtiKCommon(Handler handler, typename Handler::Context ctx, ByteReader stream) {
    auto args = B3xrrtiK::Decode(&stream);
    handler.StorePos(stream.Cursor());
    bool successful = B3xrrtiKOp7AInternal<op7a, sign>(handler, ctx, args);
    NEXT_COND(successful);
}

template <CC::Value cc, Width::Value width, typename Handler>
void B2rrd8BranchIf(Handler handler, typename Handler::Context ctx, ByteReader stream) {
    auto args = B2rrd8::Decode(&stream);
    handler.StorePos(stream.Cursor());
    int32_t delta = handler.template B2rrd8BranchIf<cc, width>(ctx, args.rr.IX(), args.rr.IY(), (int8_t) args.imm);
    stream.Advance(delta);
    NEXT;
}

template <typename Handler>
void B2xrIOpc1011SOC(Handler handler, typename Handler::Context ctx, ByteReader stream) {
    auto args = B2xrI::Decode(&stream);
    handler.StorePos(stream.Cursor());
    bool successful = B2xrIOpc1011SOCInternal(handler, ctx, args);
    NEXT_COND(successful);
}

template <CC::Value cc, ImmKind::Value immKind, Width::Value width, typename Handler>
void ExtBcc(Handler handler, typename Handler::Context ctx, ByteReader stream) {
    auto args = ExtBrr::Decode(&stream);
    handler.StorePos(stream.Cursor());
    int32_t delta = handler.template ExtBcc<cc, immKind, width>(ctx, args.rr.IX(), args.rr.IY(), args.offsetValue);
    stream.Advance(delta);
    NEXT;
}

template <typename Handler>
void ExtRet(Handler handler, typename Handler::Context ctx, ByteReader stream) {
    handler.ExtRet(ctx);
    return;
}

template <typename Handler>
void Unreachable(Handler handler, typename Handler::Context ctx, ByteReader stream) {
    // TODO: fatal error
}

/// Switch tables

template <typename Handler>
bool B2xrIOpc1011SOCInternal(Handler handler, typename Handler::Context ctx, B2xrI args) {
    switch (args.opx) {
        case Cbc::Format::B2xrI::Opc1011::NEWOBJ:
            return handler.NewObj(ctx, args.Ireg(), args.imm);
        case Cbc::Format::B2xrI::Opc1011::NEWOBJ_VST:
            ASSERTION(false, "Not implemented");
        default:
            ASSERTION(false, "Unexpected opx");
    }
    return true;
}

template <Cbc::Format::OP7A::Value op7a, Cbc::Format::Sign::Value sign, typename Handler>
bool B3xrrrOp7AInternal(Handler handler, typename Handler::Context ctx, B3xrrr args) {
    using namespace Cbc::Format;
    ASSERTION(op7a == OP7A::COMMON, "Not implemented OP7A encoding");

    auto arithOp = (Bits(sign).Shift(4) | args.opx).Raw();
    switch (arithOp) {
        case (Bits(Common::ADD ).Shift(1) | (Width::W32 & 0b1)).Raw(): return handler.template Common3R<Width::W32, Common::ADD >(ctx, args.IRegX(), args.IRegY(), args.IRegW());
        case (Bits(Common::ADD ).Shift(1) | (Width::W64 & 0b1)).Raw(): return handler.template Common3R<Width::W64, Common::ADD >(ctx, args.IRegX(), args.IRegY(), args.IRegW());
        case (Bits(Common::SUB ).Shift(1) | (Width::W32 & 0b1)).Raw(): return handler.template Common3R<Width::W32, Common::SUB >(ctx, args.IRegX(), args.IRegY(), args.IRegW());
        case (Bits(Common::SUB ).Shift(1) | (Width::W64 & 0b1)).Raw(): return handler.template Common3R<Width::W64, Common::SUB >(ctx, args.IRegX(), args.IRegY(), args.IRegW());
        case (Bits(Common::MUL ).Shift(1) | (Width::W32 & 0b1)).Raw(): return handler.template Common3R<Width::W32, Common::MUL >(ctx, args.IRegX(), args.IRegY(), args.IRegW());
        case (Bits(Common::MUL ).Shift(1) | (Width::W64 & 0b1)).Raw(): return handler.template Common3R<Width::W64, Common::MUL >(ctx, args.IRegX(), args.IRegY(), args.IRegW());
        case (Bits(Common::AND ).Shift(1) | (Width::W32 & 0b1)).Raw(): return handler.template Common3R<Width::W32, Common::AND >(ctx, args.IRegX(), args.IRegY(), args.IRegW());
        case (Bits(Common::AND ).Shift(1) | (Width::W64 & 0b1)).Raw(): return handler.template Common3R<Width::W64, Common::AND >(ctx, args.IRegX(), args.IRegY(), args.IRegW());
        case (Bits(Common::OR  ).Shift(1) | (Width::W32 & 0b1)).Raw(): return handler.template Common3R<Width::W32, Common::OR  >(ctx, args.IRegX(), args.IRegY(), args.IRegW());
        case (Bits(Common::OR  ).Shift(1) | (Width::W64 & 0b1)).Raw(): return handler.template Common3R<Width::W64, Common::OR  >(ctx, args.IRegX(), args.IRegY(), args.IRegW());
        case (Bits(Common::XOR ).Shift(1) | (Width::W32 & 0b1)).Raw(): return handler.template Common3R<Width::W32, Common::XOR >(ctx, args.IRegX(), args.IRegY(), args.IRegW());
        case (Bits(Common::XOR ).Shift(1) | (Width::W64 & 0b1)).Raw(): return handler.template Common3R<Width::W64, Common::XOR >(ctx, args.IRegX(), args.IRegY(), args.IRegW());
        case (Bits(Common::UDIV).Shift(1) | (Width::W32 & 0b1)).Raw(): return handler.template Common3R<Width::W32, Common::UDIV>(ctx, args.IRegX(), args.IRegY(), args.IRegW());
        case (Bits(Common::UDIV).Shift(1) | (Width::W64 & 0b1)).Raw(): return handler.template Common3R<Width::W64, Common::UDIV>(ctx, args.IRegX(), args.IRegY(), args.IRegW());
        case (Bits(Common::UREM).Shift(1) | (Width::W32 & 0b1)).Raw(): return handler.template Common3R<Width::W32, Common::UREM>(ctx, args.IRegX(), args.IRegY(), args.IRegW());
        case (Bits(Common::UREM).Shift(1) | (Width::W64 & 0b1)).Raw(): return handler.template Common3R<Width::W64, Common::UREM>(ctx, args.IRegX(), args.IRegY(), args.IRegW());
        case (Bits(Common::LSR ).Shift(1) | (Width::W32 & 0b1)).Raw(): return handler.template Common3R<Width::W32, Common::LSR >(ctx, args.IRegX(), args.IRegY(), args.IRegW());
        case (Bits(Common::LSR ).Shift(1) | (Width::W64 & 0b1)).Raw(): return handler.template Common3R<Width::W64, Common::LSR >(ctx, args.IRegX(), args.IRegY(), args.IRegW());
        case (Bits(Common::ASR ).Shift(1) | (Width::W32 & 0b1)).Raw(): return handler.template Common3R<Width::W32, Common::ASR >(ctx, args.IRegX(), args.IRegY(), args.IRegW());
        case (Bits(Common::ASR ).Shift(1) | (Width::W64 & 0b1)).Raw(): return handler.template Common3R<Width::W64, Common::ASR >(ctx, args.IRegX(), args.IRegY(), args.IRegW());
        case (Bits(Common::LSL ).Shift(1) | (Width::W32 & 0b1)).Raw(): return handler.template Common3R<Width::W32, Common::LSL >(ctx, args.IRegX(), args.IRegY(), args.IRegW());
        case (Bits(Common::LSL ).Shift(1) | (Width::W64 & 0b1)).Raw(): return handler.template Common3R<Width::W64, Common::LSL >(ctx, args.IRegX(), args.IRegY(), args.IRegW());
        
        default:
            ASSERTION(false, "Unexpected op");
    }
    return true;
}

template <Cbc::Format::OP7A::Value op7a, Cbc::Format::Sign::Value sign, typename Handler>
bool B3xrrtiKOp7AInternal(Handler handler, typename Handler::Context ctx, B3xrrtiK args) {
    using namespace Cbc::Format;
    ASSERTION(op7a == OP7A::COMMON, "Not implemented OP7A encoding");

    auto arithOp = (Bits(sign).Shift(4) | args.opx).Raw();
    switch (arithOp) {
        case (Bits(Common::ADD ).Shift(1) | (Width::W32 & 0b1)).Raw(): return handler.template Common3I<Width::W32, Common::ADD >(ctx, args.IRegX(), args.IRegY(), args.Imm());
        case (Bits(Common::ADD ).Shift(1) | (Width::W64 & 0b1)).Raw(): return handler.template Common3I<Width::W64, Common::ADD >(ctx, args.IRegX(), args.IRegY(), args.Imm());
        case (Bits(Common::SUB ).Shift(1) | (Width::W32 & 0b1)).Raw(): return handler.template Common3I<Width::W32, Common::SUB >(ctx, args.IRegX(), args.IRegY(), args.Imm());
        case (Bits(Common::SUB ).Shift(1) | (Width::W64 & 0b1)).Raw(): return handler.template Common3I<Width::W64, Common::SUB >(ctx, args.IRegX(), args.IRegY(), args.Imm());
        case (Bits(Common::MUL ).Shift(1) | (Width::W32 & 0b1)).Raw(): return handler.template Common3I<Width::W32, Common::MUL >(ctx, args.IRegX(), args.IRegY(), args.Imm());
        case (Bits(Common::MUL ).Shift(1) | (Width::W64 & 0b1)).Raw(): return handler.template Common3I<Width::W64, Common::MUL >(ctx, args.IRegX(), args.IRegY(), args.Imm());
        case (Bits(Common::AND ).Shift(1) | (Width::W32 & 0b1)).Raw(): return handler.template Common3I<Width::W32, Common::AND >(ctx, args.IRegX(), args.IRegY(), args.Imm());
        case (Bits(Common::AND ).Shift(1) | (Width::W64 & 0b1)).Raw(): return handler.template Common3I<Width::W64, Common::AND >(ctx, args.IRegX(), args.IRegY(), args.Imm());
        case (Bits(Common::OR  ).Shift(1) | (Width::W32 & 0b1)).Raw(): return handler.template Common3I<Width::W32, Common::OR  >(ctx, args.IRegX(), args.IRegY(), args.Imm());
        case (Bits(Common::OR  ).Shift(1) | (Width::W64 & 0b1)).Raw(): return handler.template Common3I<Width::W64, Common::OR  >(ctx, args.IRegX(), args.IRegY(), args.Imm());
        case (Bits(Common::XOR ).Shift(1) | (Width::W32 & 0b1)).Raw(): return handler.template Common3I<Width::W32, Common::XOR >(ctx, args.IRegX(), args.IRegY(), args.Imm());
        case (Bits(Common::XOR ).Shift(1) | (Width::W64 & 0b1)).Raw(): return handler.template Common3I<Width::W64, Common::XOR >(ctx, args.IRegX(), args.IRegY(), args.Imm());
        case (Bits(Common::UDIV).Shift(1) | (Width::W32 & 0b1)).Raw(): return handler.template Common3I<Width::W32, Common::UDIV>(ctx, args.IRegX(), args.IRegY(), args.Imm());
        case (Bits(Common::UDIV).Shift(1) | (Width::W64 & 0b1)).Raw(): return handler.template Common3I<Width::W64, Common::UDIV>(ctx, args.IRegX(), args.IRegY(), args.Imm());
        case (Bits(Common::UREM).Shift(1) | (Width::W32 & 0b1)).Raw(): return handler.template Common3I<Width::W32, Common::UREM>(ctx, args.IRegX(), args.IRegY(), args.Imm());
        case (Bits(Common::UREM).Shift(1) | (Width::W64 & 0b1)).Raw(): return handler.template Common3I<Width::W64, Common::UREM>(ctx, args.IRegX(), args.IRegY(), args.Imm());
        case (Bits(Common::LSR ).Shift(1) | (Width::W32 & 0b1)).Raw(): return handler.template Common3I<Width::W32, Common::LSR >(ctx, args.IRegX(), args.IRegY(), args.Imm());
        case (Bits(Common::LSR ).Shift(1) | (Width::W64 & 0b1)).Raw(): return handler.template Common3I<Width::W64, Common::LSR >(ctx, args.IRegX(), args.IRegY(), args.Imm());
        case (Bits(Common::ASR ).Shift(1) | (Width::W32 & 0b1)).Raw(): return handler.template Common3I<Width::W32, Common::ASR >(ctx, args.IRegX(), args.IRegY(), args.Imm());
        case (Bits(Common::ASR ).Shift(1) | (Width::W64 & 0b1)).Raw(): return handler.template Common3I<Width::W64, Common::ASR >(ctx, args.IRegX(), args.IRegY(), args.Imm());
        case (Bits(Common::LSL ).Shift(1) | (Width::W32 & 0b1)).Raw(): return handler.template Common3I<Width::W32, Common::LSL >(ctx, args.IRegX(), args.IRegY(), args.Imm());
        case (Bits(Common::LSL ).Shift(1) | (Width::W64 & 0b1)).Raw(): return handler.template Common3I<Width::W64, Common::LSL >(ctx, args.IRegX(), args.IRegY(), args.Imm());

        default:
            ASSERTION(false, "Unexpected op");
    }
    return true;
}

}

#endif // CBC_DISPATCHER_H
