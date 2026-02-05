// is not supposed to be included anywhere outside interpreter.cpp

#include "cbc/isa.h"
#include "cbc/isa_rt.h"
#include "interpreter/ectype.h"
#include "interpreter/literals.h"
#include "utils/math.h"
#include "interpreter/runtime.h"

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
            int64_t left = static_cast<int64_t>(l.u64);
            int64_t right = static_cast<int64_t>(r.u64);
            if (right == 0) {
                return {l, false};
            }
            if (left == INT64_MIN && right == -1) {
                return {l, true};
            }
            return {Value::Primitive{ .u64 = static_cast<uint64_t>(left / right) }, true};
        }
        case Common::SREM: {
            int64_t left = static_cast<int64_t>(l.u64);
            int64_t right = static_cast<int64_t>(r.u64);
            if (right == 0) {
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
            int32_t left = static_cast<int32_t>(l.u32);
            int32_t right = static_cast<int32_t>(r.u32);
            if (right == 0) {
                return {l, false};
            }
            if (left == INT32_MIN && right == -1) {
                return {l, true};
            }
            return {Value::Primitive{ .u32 = static_cast<uint32_t>(left / right) }, true};
        }
        case Common::SREM: {
            int32_t left = static_cast<int32_t>(l.u32);
            int32_t right = static_cast<int32_t>(r.u32);
            if (right == 0) {
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

template <ImmKind::Value immKind>
static inline uint64_t DecodeImmediate(LiteralTable* literals, uint16_t value);

template <>
inline uint64_t DecodeImmediate<ImmKind::VALUE>(LiteralTable* literals, uint16_t value) {
    return MathUtils::SignExtend(value, 12);
}

template <>
inline uint64_t DecodeImmediate<ImmKind::LITERAL>(LiteralTable* literals, uint16_t value) {
    return literals->at(value).u64;
}

template <typename RTI>
class MemoryLocation {
public:

    inline MemoryLocation(uint8_t* _base, size_t _offset) : base(_base), offset(_offset) {}
    inline MemoryLocation(uintptr_t _base, size_t _offset)
        : base(reinterpret_cast<uint8_t*>(_base)), offset(_offset) {}

    inline void StorePrim(StoreAccessKind::Value stk, RT::Reg src, Ectype* ectype) {
        switch (stk) {
            case StoreAccessKind::ST_8: Store<uint8_t>(src, ectype); return;
            case StoreAccessKind::ST_16: Store<uint16_t>(src, ectype); return;
            case StoreAccessKind::ST_32: Store<uint32_t>(src, ectype); return;
            case StoreAccessKind::ST_64: Store<uint64_t>(src, ectype); return;
            case StoreAccessKind::ST_F32: Store<float>(src, ectype); return;
            case StoreAccessKind::ST_F64: Store<double>(src, ectype); return;
            default: ASSERTION(false, "Unexpected stk");
        }
    }

    inline void LoadPrim(LoadAccessKind::Value ldk, RT::Reg dst, Ectype* ectype) {
        switch (ldk) {
            case LoadAccessKind::LD_U8:  Load<uint8_t>(dst, ectype); return;
            case LoadAccessKind::LD_U16: Load<uint16_t>(dst, ectype); return;
            case LoadAccessKind::LD_32: Load<uint32_t>(dst, ectype); return;
            case LoadAccessKind::LD_64: Load<uint64_t>(dst, ectype); return;
            case LoadAccessKind::LD_S8:  Load<int8_t>(dst, ectype); return;
            case LoadAccessKind::LD_S16: Load<int16_t>(dst, ectype); return;
            case LoadAccessKind::LD_S32TO64: Load<int32_t>(dst, ectype); return;
            case LoadAccessKind::LD_F32: Load<float>(dst, ectype); return;
            case LoadAccessKind::LD_F64: Load<double>(dst, ectype); return;
            default: ASSERTION(false, "Unexpected ldk");
        }
    }

private:
    template <typename P>
    inline void Store(RT::Reg src, Ectype* ectype) {
        *reinterpret_cast<P*>(base + offset) = static_cast<P>(ectype->GetPrimitive(src.IR()).u64);
    }

    template <>
    inline void Store<float>(RT::Reg src, Ectype* ectype) {
        *reinterpret_cast<float*>(base + offset) = static_cast<float>(ectype->GetPrimitive(src.FR()).f32);
    }

    template <>
    inline void Store<double>(RT::Reg src, Ectype* ectype) {
        *reinterpret_cast<double*>(base + offset) = static_cast<double>(ectype->GetPrimitive(src.FR()).f32);
    }

    template <typename P>
    inline void Load(RT::Reg dst, Ectype* ectype) {
        auto value = *reinterpret_cast<P*>(base + offset);
        ectype->Put(dst.IR(), Value::Primitive {
            .u64 = static_cast<uint64_t>(value)
        });
    }

    template <>
    inline void Load<float>(RT::Reg dst, Ectype* ectype) {
        auto value = *reinterpret_cast<float*>(base + offset);
        ectype->Put(dst.FR(), Value::Primitive {
            .f32 = value
        });
    }

    template <>
    inline void Load<double>(RT::Reg dst, Ectype* ectype) {
        auto value = *reinterpret_cast<double*>(base + offset);
        ectype->Put(dst.FR(), Value::Primitive {
            .f64 = value
        });
    }

    uint8_t* base;
    size_t offset;
};

} // namespace Interpretation
