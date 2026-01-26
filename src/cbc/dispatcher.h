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
using Common = Cbc::Format::Common;
using CC = Cbc::Format::CC;
using ImmKind = Cbc::Format::ImmKind;

template <typename Handler>
void Unreachable(Handler handler, typename Handler::Context ctx, ByteReader stream);

template <typename Handler, Width::Value width>
void B2rrMovPrimitive(Handler handler, typename Handler::Context ctx, ByteReader stream);

template <typename Handler>
void B2rrMovRef(Handler handler, typename Handler::Context ctx, ByteReader stream);

template <typename Handler>
void B2rrMovVST(Handler handler, typename Handler::Context ctx, ByteReader stream);

template <Common::Value arithOp, Width::Value width, typename Handler>
void B2rrCommon(Handler handler, typename Handler::Context ctx, ByteReader stream);

template <CC::Value cc, Width::Value width, typename Handler>
void B2rrd8BranchIf(Handler handler, typename Handler::Context ctx, ByteReader stream);

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
        table_put(Format::B2rr::Fmt(Common::MOV, width), B2rrCommon<Common::MOV, width, Handler>);
    });

    table_put(Format::B2rr::Fmt(Common::MVST, Width::W32), B2rrMovVST);
    table_put(Format::B2rr::Fmt(Common::MREF, Width::W64), B2rrMovRef);

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

    return arr;
}();

template <typename Handler, Width::Value width>
void B2rrMovPrimitive(Handler handler, typename Handler::Context ctx, ByteReader stream) {
    auto args = B2rr::Decode(&stream);
    handler.StorePos(stream.Cursor());
    handler.template Mov<width>(ctx, args.dst, args.src);
    NEXT;
}

template <typename Handler>
void B2rrMovRef(Handler handler, typename Handler::Context ctx, ByteReader stream) {
    auto args = B2rr::Decode(&stream);
    handler.StorePos(stream.Cursor());
    handler.MovRef(ctx, args.dst, args.src);
    NEXT;
}

template <typename Handler>
void B2rrMovVST(Handler handler, typename Handler::Context ctx, ByteReader stream) {
    auto args = B2rr::Decode(&stream);
    handler.StorePos(stream.Cursor());
    handler.MovVST(ctx, args.dst, args.src);
    NEXT;
}

template <Common::Value arithOp, Width::Value width, typename Handler>
void B2rrCommon(Handler handler, typename Handler::Context ctx, ByteReader stream) {
    auto args = B2rr::Decode(&stream);
    handler.StorePos(stream.Cursor());
    bool successful = handler.template Common<width, arithOp>(ctx, args.Idst(), args.Isrc());
    NEXT_COND(successful);
}

template <CC::Value cc, Width::Value width, typename Handler>
void B2rrd8BranchIf(Handler handler, typename Handler::Context ctx, ByteReader stream) {
    auto args = B2rrd8::Decode(&stream);
    handler.StorePos(stream.Cursor());
    int32_t delta = handler.template B2rrd8BranchIf<cc, width>(ctx, args.rr.IX(), args.rr.IY(), (int8_t) args.byte);
    stream.Advance(delta);
    NEXT;
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

}

#endif // CBC_DISPATCHER_H
