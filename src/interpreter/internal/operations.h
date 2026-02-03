// is not supposed to be included anywhere outside interpreter.cpp

#include "cbc/isa.h"
#include "interpreter/ectype.h"
#include "interpreter/literals.h"

namespace Interpretation {

using namespace Cbc::Format;

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
        case Common::LSR:  return {Value::Primitive{ .u64 = l.u64 >> (r.u64 & 0x3F) }, true};
        case Common::LSL:  return {Value::Primitive{ .u64 = l.u64 << (r.u64 & 0x3F) }, true};

        case Common::ASR: {
            int64_t left = static_cast<int64_t>(l.u64);
            return {Value::Primitive{ .u64 = static_cast<uint64_t>(left >> (r.u64 & 0x3F)) }, true};
        }

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
            int64_t left = (int64_t) l.u64;
            int64_t right = (int64_t) l.u64;
            if (left == 0) {
                return {l, false};
            }
            if (left == INT64_MIN && right == -1) {
                return {l, true};
            }
            return {Value::Primitive{ .u64 = static_cast<uint64_t>(left / right) }, true};
        }
        case Common::SREM: {
            int64_t left = (int64_t) l.u64;
            int64_t right = (int64_t) l.u64;
            if (left == 0) {
                return {l, false};
            }
            if (left == INT64_MIN && right == -1) {
                return {Value::Primitive{ .u64 = 0 }, true};
            }
            return {Value::Primitive{ .u64 = static_cast<uint64_t>(left / right) }, true};
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
        case Common::LSR:  return {Value::Primitive{ .u32 = l.u32 >> (r.u32 & 0x1F) }, true};
        case Common::LSL:  return {Value::Primitive{ .u32 = l.u32 << (r.u32 & 0x1F) }, true};

        case Common::ASR: {
            int32_t left = static_cast<int32_t>(l.u32);
            return {Value::Primitive{ .u32 = static_cast<uint32_t>(left >> (r.u32 & 0x1F)) }, true};
        }

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
            int32_t left = (int32_t) l.u32;
            int32_t right = (int32_t) l.u32;
            if (left == 0) {
                return {l, false};
            }
            if (left == INT32_MIN && right == -1) {
                return {l, true};
            }
            return {Value::Primitive{ .u32 = static_cast<uint32_t>(left / right) }, true};
        }
        case Common::SREM: {
            int32_t left = (int32_t) l.u32;
            int32_t right = (int32_t) l.u32;
            if (left == 0) {
                return {l, false};
            }
            if (left == INT32_MIN && right == -1) {
                return {Value::Primitive{ .u32 = 0 }, true};
            }
            return {Value::Primitive{ .u32 = static_cast<uint32_t>(left / right) }, true};
        }
    }
}

template <CC::Value cc, Width::Value width>
inline static bool Compare(Value::Primitive l, Value::Primitive r);

template <> inline bool Compare<CC::EQ, Width::W32>(Value::Primitive l, Value::Primitive r) { return l.u32 == r.u32; }
template <> inline bool Compare<CC::NE, Width::W32>(Value::Primitive l, Value::Primitive r) { return l.u32 != r.u32; }
template <> inline bool Compare<CC::LT, Width::W32>(Value::Primitive l, Value::Primitive r) { return static_cast<int32_t>(l.u32) < static_cast<int32_t>(r.u32); }
template <> inline bool Compare<CC::GE, Width::W32>(Value::Primitive l, Value::Primitive r) { return static_cast<int32_t>(l.u32) >= static_cast<int32_t>(r.u32); }
template <> inline bool Compare<CC::ULT, Width::W32>(Value::Primitive l, Value::Primitive r) { return l.u32 < r.u32; }
template <> inline bool Compare<CC::UGE, Width::W32>(Value::Primitive l, Value::Primitive r) { return l.u32 >= r.u32; }
template <> inline bool Compare<CC::TESTZ, Width::W32>(Value::Primitive l, Value::Primitive r) { return (l.u32 & r.u32) == 0; }
template <> inline bool Compare<CC::TESTNZ, Width::W32>(Value::Primitive l, Value::Primitive r) { return (l.u32 & r.u32) != 0; }
template <> inline bool Compare<CC::EQ, Width::W64>(Value::Primitive l, Value::Primitive r) { return l.u64 == r.u64; }
template <> inline bool Compare<CC::NE, Width::W64>(Value::Primitive l, Value::Primitive r) { return l.u64 != r.u64; }
template <> inline bool Compare<CC::LT, Width::W64>(Value::Primitive l, Value::Primitive r) { return static_cast<int64_t>(l.u64) < static_cast<int64_t>(r.u64); }
template <> inline bool Compare<CC::GE, Width::W64>(Value::Primitive l, Value::Primitive r) { return static_cast<int64_t>(l.u64) >= static_cast<int64_t>(r.u64); }
template <> inline bool Compare<CC::ULT, Width::W64>(Value::Primitive l, Value::Primitive r) { return l.u64 < r.u64; }
template <> inline bool Compare<CC::UGE, Width::W64>(Value::Primitive l, Value::Primitive r) { return l.u64 >= r.u64; }
template <> inline bool Compare<CC::TESTZ, Width::W64>(Value::Primitive l, Value::Primitive r) { return (l.u64 & r.u64) == 0; }
template <> inline bool Compare<CC::TESTNZ, Width::W64>(Value::Primitive l, Value::Primitive r) { return (l.u64 & r.u64) != 0; }


template <CC::Value cc, Width::Value width>
inline static bool Compare(Value::Reference l, Value::Reference r) {
    ASSERTION(false, "Unreachable");
    return false;
}

template <> inline bool Compare<CC::REQ, Width::W64>(Value::Reference l, Value::Reference r) { return l.value == r.value; }
template <> inline bool Compare<CC::RNE, Width::W32>(Value::Reference l, Value::Reference r) { return l.value != r.value; }


template <Width::Value width>
static inline bool Compare(CC cc, Value::Primitive l, Value::Primitive r) {
}

template <ImmKind::Value immKind>
static inline int32_t JumpOffset(LiteralTable* literals, uint16_t value);

template <>
inline int32_t JumpOffset<ImmKind::VALUE>(LiteralTable* literals, uint16_t value) {
    return (int32_t) ((int16_t) value);
}

template <>
inline int32_t JumpOffset<ImmKind::LITERAL>(LiteralTable* literals, uint16_t value) {
    return literals->at(value).i32;
}

} // namespace Interpretation
