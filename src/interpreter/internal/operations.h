// is not supposed to be included anywhere outside interpreter.cpp

#include "cbc/isa.h"
#include "interpreter/ectype.h"
#include "interpreter/literals.h"

namespace Interpretation {

using namespace Cbc::Format;
using ThreadHandle = void*;

struct ArithmeticResult {
    Value::Primitive result;
    bool successful;
};

template <Width::Value width>
static inline ArithmeticResult Arith(Common::Value op, Value::Primitive l, Value::Primitive r);

template <>
inline ArithmeticResult Arith<Width::W64>(Common::Value op, Value::Primitive l, Value::Primitive r) {
    using namespace Cbc::Format;
    switch (op) {
        case Common::ADD:  return {Value::Primitive{ .u64 = l.u64 + r.u64 }, true};
        case Common::SUB:  return {Value::Primitive{ .u64 = l.u64 - r.u64 }, true};
        case Common::MUL:  return {Value::Primitive{ .u64 = l.u64 * r.u64 }, true};
        case Common::AND:  return {Value::Primitive{ .u64 = l.u64 & r.u64 }, true};
        case Common::OR:   return {Value::Primitive{ .u64 = l.u64 | r.u64 }, true};
        case Common::XOR:  return {Value::Primitive{ .u64 = l.u64 & r.u64 }, true};
        case Common::ASR:  return {Value::Primitive{ .i64 = l.i64 >> (r.u64 & 0x3F) }, true};
        case Common::LSR:  return {Value::Primitive{ .u64 = l.u64 >> (r.u64 & 0x3F) }, true};
        case Common::LSL:  return {Value::Primitive{ .u64 = l.u64 << (r.u64 & 0x3F) }, true};

        case Common::UDIV: {
            if (r.u64 == 0) {
                return {l, false};
            }
            return {Value::Primitive{ .u64 = l.u64 / r.u64 }, true};
        }
        case Common::UREM: {
            if (r.u64 == 0) {
                return {l, false};
            }
            return {Value::Primitive{ .u64 = l.u64 % r.u64 }, true};
        }

        case Common::SDIV: {
            if (r.i64 == 0) {
                return {l, false};
            }
            if (l.i64 == INT64_MIN && r.i64 == -1) {
                return {l, true};
            }
            return {Value::Primitive{ .i64 = l.i64 / r.i64 }, true};
        }
        case Common::SREM: {
            if (r.i64 == 0) {
                return {l, false};
            }
            if (l.i64 == INT64_MIN && r.i64 == -1) {
                return {Value::Primitive{ .i64 = 0 }, true};
            }
            return {Value::Primitive{ .i64 = l.i64 % r.i64 }, true};
        }
    }
}

template <>
inline ArithmeticResult Arith<Width::W32>(Common::Value op, Value::Primitive l, Value::Primitive r) {
    using namespace Cbc::Format;
    switch (op) {
        case Common::ADD:  return {Value::Primitive{ .u32 = l.u32 + r.u32 }, true};
        case Common::SUB:  return {Value::Primitive{ .u32 = l.u32 - r.u32 }, true};
        case Common::MUL:  return {Value::Primitive{ .u32 = l.u32 * r.u32 }, true};
        case Common::AND:  return {Value::Primitive{ .u32 = l.u32 & r.u32 }, true};
        case Common::OR:   return {Value::Primitive{ .u32 = l.u32 | r.u32 }, true};
        case Common::XOR:  return {Value::Primitive{ .u32 = l.u32 & r.u32 }, true};
        case Common::ASR:  return {Value::Primitive{ .i32 = l.i32 >> (r.u32 & 0x1F) }, true};
        case Common::LSR:  return {Value::Primitive{ .u32 = l.u32 >> (r.u32 & 0x1F) }, true};
        case Common::LSL:  return {Value::Primitive{ .u32 = l.u32 << (r.u32 & 0x1F) }, true};

        case Common::UDIV: {
            if (r.u32 == 0) {
                return {l, false};
            }
            return {Value::Primitive{ .u32 = l.u32 / r.u32 }, true};
        }
        case Common::UREM: {
            if (r.u32 == 0) {
                return {l, false};
            }
            return {Value::Primitive{ .u32 = l.u32 % r.u32 }, true};
        }

        case Common::SDIV: {
            if (r.i32 == 0) {
                return {l, false};
            }
            if (l.i32 == INT32_MIN && r.i32 == -1) {
                return {l, true};
            }
            return {Value::Primitive{ .i32 = l.i32 / r.i32 }, true};
        }
        case Common::SREM: {
            if (r.i32 == 0) {
                return {l, false};
            }
            if (l.i32 == INT32_MIN && r.i32 == -1) {
                return {Value::Primitive{ .i32 = 0 }, true};
            }
            return {Value::Primitive{ .i32 = l.i32 % r.i32 }, true};
        }
    }
}

template <CC::Value cc, Width::Value width>
inline static bool Compare(Value::Primitive l, Value::Primitive r);

template <> inline bool Compare<CC::EQ, Width::W32>(Value::Primitive l, Value::Primitive r) { return l.i32 == r.i32; }
template <> inline bool Compare<CC::NE, Width::W32>(Value::Primitive l, Value::Primitive r) { return l.i32 != r.i32; }
template <> inline bool Compare<CC::LT, Width::W32>(Value::Primitive l, Value::Primitive r) { return l.i32 < r.i32; }
template <> inline bool Compare<CC::GE, Width::W32>(Value::Primitive l, Value::Primitive r) { return l.i32 >= r.i32; }
template <> inline bool Compare<CC::ULT, Width::W32>(Value::Primitive l, Value::Primitive r) { return l.u32 < r.u32; }
template <> inline bool Compare<CC::UGE, Width::W32>(Value::Primitive l, Value::Primitive r) { return l.u32 >= r.u32; }
template <> inline bool Compare<CC::TESTZ, Width::W32>(Value::Primitive l, Value::Primitive r) { return (l.u32 & r.u32) == 0; }
template <> inline bool Compare<CC::TESTNZ, Width::W32>(Value::Primitive l, Value::Primitive r) { return (l.u32 & r.u32) != 0; }
template <> inline bool Compare<CC::EQ, Width::W64>(Value::Primitive l, Value::Primitive r) { return l.i64 == r.i64; }
template <> inline bool Compare<CC::NE, Width::W64>(Value::Primitive l, Value::Primitive r) { return l.i64 != r.i64; }
template <> inline bool Compare<CC::LT, Width::W64>(Value::Primitive l, Value::Primitive r) { return l.i64 < r.i64; }
template <> inline bool Compare<CC::GE, Width::W64>(Value::Primitive l, Value::Primitive r) { return l.i64 >= r.i64; }
template <> inline bool Compare<CC::ULT, Width::W64>(Value::Primitive l, Value::Primitive r) { return l.u64 < r.u64; }
template <> inline bool Compare<CC::UGE, Width::W64>(Value::Primitive l, Value::Primitive r) { return l.u64 >= r.u64; }
template <> inline bool Compare<CC::TESTZ, Width::W64>(Value::Primitive l, Value::Primitive r) { return (l.u64 & r.u64) == 0; }
template <> inline bool Compare<CC::TESTNZ, Width::W64>(Value::Primitive l, Value::Primitive r) { return (l.u64 & r.u64) != 0; }


template <CC::Value cc>
inline static bool Compare(Value::Reference l, Value::Reference r);
template <> inline bool Compare<CC::REQ>(Value::Reference l, Value::Reference r) { return l.value == r.value; }
template <> inline bool Compare<CC::RNE>(Value::Reference l, Value::Reference r) { return l.value != r.value; }

template <ImmKind::Value immKind>
static inline int32_t JumpOffset(LiteralTable* literals, uint16_t value);

template <>
inline int32_t JumpOffset<ImmKind::VALUE>(LiteralTable* literals, uint16_t value) {
    return (int32_t) ((int16_t) value);
}

template <>
inline int32_t JumpOffset<ImmKind::LITERAL>(LiteralTable* literals, uint16_t value) {
    return literals->table[value].i32;
}

} // namespace Interpretation
