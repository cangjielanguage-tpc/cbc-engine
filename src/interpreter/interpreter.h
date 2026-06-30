#pragma once

#include "ectype.h"
#include "frame.h"
#include "literals.h"
#include "runtimesupport/runtime.h"

#include "cbc/isa_rt.h"
#include "internal/operations.h"

namespace Interpretation {

class Interpreter {
    using IReg = Cbc::IReg;

public:
    Interpreter(Ectype* _ectype, Frame _frame, RTSupport::ThreadHandle _handle, LiteralTable* _literals)
        : ectype(_ectype),
          frame(_frame),
          handle(_handle),
          literals(_literals)
    {}

    inline void StorePos(uint8_t* c) { /* no-op */ }

    template <Width::Value width> inline bool Binary(Common::Value arithOp, IReg d, IReg l, IReg r)
    {
        return Binary<width>(arithOp, d, l, ectype->GetPrimitive(r));
    }

    template <RT::ImmKind::Value immKind, Width::Value width>
    inline bool BinaryImm(Common::Value arithOp, IReg d, IReg l, uint16_t imm)
    {
        return Binary<width>(arithOp, d, l, Value::Primitive { .u64 = DecodeImmediate<immKind>(literals, imm) });
    }

    template <Width::Value width> inline bool Binary(Common::Value arithOp, IReg d, IReg l, Value::Primitive val)
    {
        auto res = Arith<width>(arithOp, ectype->GetPrimitive(l), val);
        if (res.successful) {
            ectype->Put(d, res.result);
            return true;
        }
        return false;
    }

    template <Width::Value width> inline bool Binary(FloatOperations::Value fpOp, FReg d, FReg l, FReg r)
    {
        auto res = ArithFP<width>(fpOp, ectype->GetPrimitive(l), ectype->GetPrimitive(r));
        if (res.successful) {
            ectype->Put(d, res.result);
            return true;
        }
        return false;
    }

    template <Width::Value width> inline bool Unary(FloatOperations::Value fpOp, FReg d, FReg s)
    {
        auto res = ArithFP<width>(fpOp, ectype->GetPrimitive(s));
        if (res.successful) {
            ectype->Put(d, res.result);
            return true;
        }
        return false;
    }

    inline bool LoadAddr(Format::LoadAccessKind ldk, Format::Reg dst, uint64_t location)
    {
        if (ldk == LoadAccessKind::LD_REF) {
            ectype->Put(dst.IR(), RTSupport::Execution::ReadObjectStatic(reinterpret_cast<void*>(location), handle));
        } else {
            MemoryLocation(location).LoadPrim(ldk, dst, ectype);
        }
        return true;
    }

    inline bool StoreAddr(Format::StoreAccessKind stk, Format::Reg src, uint64_t location)
    {
        if (stk == StoreAccessKind::ST_REF) {
            RTSupport::Execution::WriteObjectStatic(
                reinterpret_cast<void*>(location), ectype->GetReference(src.IR()), handle
            );
        } else {
            MemoryLocation(location).StorePrim(stk, src, ectype);
        }
        return true;
    }

    inline bool LoadObj(Format::LoadAccessKind ldk, Format::Reg dst, IReg base, uint64_t offset)
    {
        auto obj = ectype->GetReference(base);
        if (!NullCheck(obj)) {
            return false;
        }
        if (ldk == LoadAccessKind::LD_REF) {
            ectype->Put(dst.IR(), RTSupport::Execution::ReadObjectInstance(obj, obj.value + offset, handle));
        } else {
            MemoryLocation(obj.value, offset).LoadPrim(ldk, dst, ectype);
        }
        return true;
    }

    inline bool StoreObj(Format::StoreAccessKind stk, Format::Reg src, IReg base, uint64_t offset)
    {
        auto obj = ectype->GetReference(base);
        if (!NullCheck(obj)) {
            return false;
        }
        if (stk == StoreAccessKind::ST_REF) {
            RTSupport::Execution::WriteObjectInstance(obj, obj.value + offset, ectype->GetReference(src.IR()), handle);
        } else {
            MemoryLocation(obj.value, offset).StorePrim(stk, src, ectype);
        }
        return true;
    }

    inline bool StoreObjImm(Format::StoreAccessKind stk, IReg base, uint64_t offset, uint64_t imm)
    {
        auto obj = ectype->GetReference(base);
        if (!NullCheck(obj)) {
            return false;
        }
        if (stk == StoreAccessKind::ST_REF) {
            return false;
        } else {
            MemoryLocation(obj.value, offset).StoreImm(stk, imm);
        }
        return true;
    }

    inline bool LoadDerived(Format::LoadAccessKind ldk, Format::Reg dst, IReg base, IReg derived, uint64_t offset)
    {
        auto obj = ectype->GetReference(base);
        auto derivedAddr = ectype->GetPrimitive(derived).u64;
        auto locationKind = RTSupport::Execution::GetStructLocationKind(obj, derivedAddr);
        switch (locationKind) {
            case RTSupport::LOCAL:  return LoadRec(ldk, dst, base, offset);
            case RTSupport::GLOBAL: return LoadRec(ldk, dst, IReg::IRZ, derivedAddr + offset);
            case RTSupport::HEAP:   return LoadObj(ldk, dst, base, derived + offset);
        }
    }

    inline bool StoreDerived(Format::StoreAccessKind stk, Format::Reg src, IReg base, IReg derived, uint64_t offset)
    {
        auto obj = ectype->GetReference(base);
        auto derivedAddr = ectype->GetPrimitive(derived).u64;
        auto locationKind = RTSupport::Execution::GetStructLocationKind(obj, derivedAddr);
        switch (locationKind) {
            case RTSupport::LOCAL:  return StoreRec(stk, src, IReg::IRZ, derivedAddr + offset);
            case RTSupport::GLOBAL: return StoreRec(stk, src, derived, offset);
            case RTSupport::HEAP:   return StoreObj(stk, src, base, (derivedAddr - obj.value) + offset);
        }
    }

    inline bool StoreDerivedImm(Format::StoreAccessKind stk, IReg base, IReg derived, uint64_t offset, uint64_t imm)
    {
        auto obj         = ectype->GetReference(base);
        auto derivedAddr = ectype->GetPrimitive(derived).u64;
        auto locationKind = RTSupport::Execution::GetStructLocationKind(obj, derivedAddr);
        switch (locationKind) {
            case RTSupport::LOCAL:  return StoreRecImm(stk, IReg::IRZ, derivedAddr + offset, imm);
            case RTSupport::GLOBAL: return StoreRecImm(stk, derived, offset, imm);
            case RTSupport::HEAP:   return StoreObjImm(stk, base, (derivedAddr - obj.value) + offset, imm);
        }
    }

    inline bool LoadArray(Format::LoadAccessKind ldk, Format::Reg dst, IReg base, IReg idx)
    {
        auto obj = ectype->GetReference(base);
        if (!NullCheck(obj)) {
            return false;
        }
        if (ldk == LoadAccessKind::LD_REF) {
            ectype->Put(dst.IR(), RTSupport::Execution::ReadArrayElem(obj, ectype->GetPrimitive(idx).u64, handle));
        } else {
            auto offset = CalcLoadArrayOffset(ldk, idx, ectype);
            MemoryLocation(obj.value, offset).LoadPrim(ldk, dst, ectype);
        }
        return true;
    }

    inline bool StoreArray(Format::StoreAccessKind stk, Format::Reg src, IReg base, IReg idx)
    {
        auto obj = ectype->GetReference(base);
        if (!NullCheck(obj)) {
            return false;
        }
        if (stk == StoreAccessKind::ST_REF) {
            RTSupport::Execution::WriteArrayElem(
                obj, ectype->GetPrimitive(idx).u64, ectype->GetReference(src.IR()), handle
            );
        } else {
            auto offset = CalcStoreArrayOffset(stk, idx, ectype);
            MemoryLocation(obj.value, offset).StorePrim(stk, src, ectype);
        }
        return true;
    }

    inline bool LoadRec(Format::LoadAccessKind ldk, Format::Reg dst, IReg base, size_t offset)
    {
        auto ptr = static_cast<uintptr_t>(ectype->GetPrimitive(base).u64);
        // IRZ means static record field, so whole position is encoded in accumulated offset
        // FIXME: encode as separate operation
        if (base != IReg::IRZ && ptr == 0) {
            return false;
        }
        if (ldk == LoadAccessKind::LD_REF) {
            if (base == IReg::IRZ) {
                // Static record field requires barrier
                ectype->Put(dst.IR(), RTSupport::Execution::ReadObjectStatic(reinterpret_cast<void*>(offset), handle));
            } else {
                MemoryLocation(ptr, offset).LoadRef(dst, ectype);
            }
        } else {
            MemoryLocation(ptr, offset).LoadPrim(ldk, dst, ectype);
        }
        return true;
    }

    inline bool StoreRec(Format::StoreAccessKind stk, Format::Reg src, IReg base, uint64_t offset)
    {
        auto ptr = static_cast<uintptr_t>(ectype->GetPrimitive(base).u64);
        // IRZ means static record field, so whole position is encoded in accumulated offset
        // FIXME: encode as separate operation
        if (base != IReg::IRZ && ptr == 0) {
            return false;
        }
        if (stk == StoreAccessKind::ST_REF) {
            auto ref = ectype->GetReference(src.IR());
            if (base == IReg::IRZ) {
                // Static record field requires barrier
                RTSupport::Execution::WriteObjectStatic(reinterpret_cast<void*>(offset), ref, handle);
            } else {
                MemoryLocation(ptr, offset).StoreRef(src, ectype);
            }
        } else {
            MemoryLocation(ptr, offset).StorePrim(stk, src, ectype);
        }
        return true;
    }

    inline bool StoreRecImm(Format::StoreAccessKind stk, IReg base, uint64_t offset, uint64_t imm)
    {
        auto ptr = static_cast<uintptr_t>(ectype->GetPrimitive(base).u64);
        // IRZ means static record field, so whole position is encoded in accumulated offset
        if (base != IReg::IRZ && ptr == 0) {
            return false;
        }
        ASSERTION(stk <= Format::StoreAccessKind::ST_64, "Unexpected store access kind");
        MemoryLocation(ptr, offset).StoreImm(stk, imm);
        return true;
    }

    inline bool LoadFrame(Format::LoadAccessKind ldk, Format::Reg dst, size_t offset)
    {
        auto ptr = frame.start;
        if (ldk == LoadAccessKind::LD_REF) {
            MemoryLocation(ptr, offset).LoadRef(dst, ectype);
        } else {
            MemoryLocation(ptr, offset).LoadPrim(ldk, dst, ectype);
        }
        return true;
    }

    inline bool StoreFrame(Format::StoreAccessKind stk, Format::Reg src, uint64_t offset)
    {
        auto ptr = frame.start;
        if (stk == StoreAccessKind::ST_REF) {
            MemoryLocation(ptr, offset).StoreRef(src, ectype);
        } else {
            MemoryLocation(ptr, offset).StorePrim(stk, src, ectype);
        }
        return true;
    }

    inline bool StoreFrameImm(Format::StoreAccessKind stk, uint64_t imm, uint64_t offset)
    {
        auto ptr = frame.start;
        ASSERTION(stk <= Format::StoreAccessKind::ST_64, "Unexpected store access kind");
        MemoryLocation(ptr, offset).StoreImm(stk, imm);
        return true;
    }

    inline void MovRef(IReg d, IReg s) { ectype->Put(d, ectype->GetReference(s)); }

    template <typename ToType, typename FromType> inline void Mov(ToType d, FromType s)
    {
        ectype->Put(d, ectype->GetPrimitive(s));
    }

    inline void MovI(IReg d, uint64_t imm) { ectype->Put(d, Value::Primitive { .u64 = imm }); }

    inline void MovI(FReg d, float imm) { ectype->Put(d, Value::Primitive { .f32 = imm }); }

    inline void MovI(FReg d, double imm) { ectype->Put(d, Value::Primitive { .f64 = imm }); }

    template <Width::Value width> inline bool Cmp(CC cc, IReg l, IReg r)
    {
        if (cc.IsRef()) {
            return CmpRef<width>(cc, l, ectype->GetReference(r));
        } else {
            return CmpPrim<width>(cc, l, ectype->GetPrimitive(r));
        }
    }

    template <Width::Value width> inline bool CmpPrim(CC cc, IReg l, Value::Primitive r)
    {
        switch (cc) {
            case CC::EQ:     return Compare<CC::EQ, width>(ectype->GetPrimitive(l), r);
            case CC::NE:     return Compare<CC::NE, width>(ectype->GetPrimitive(l), r);
            case CC::LT:     return Compare<CC::LT, width>(ectype->GetPrimitive(l), r);
            case CC::GE:     return Compare<CC::GE, width>(ectype->GetPrimitive(l), r);
            case CC::ULT:    return Compare<CC::ULT, width>(ectype->GetPrimitive(l), r);
            case CC::UGE:    return Compare<CC::UGE, width>(ectype->GetPrimitive(l), r);
            case CC::TESTZ:  return Compare<CC::TESTZ, width>(ectype->GetPrimitive(l), r);
            case CC::TESTNZ: return Compare<CC::TESTNZ, width>(ectype->GetPrimitive(l), r);
            default:         FATAL("Unreachable"); return false;
        }
    }

    template <Width::Value width> inline bool CmpRef(CC cc, IReg l, Value::Reference r)
    {
        switch (cc) {
            case CC::REQ: return Compare<CC::REQ, width>(ectype->GetReference(l), r);
            case CC::RNE: return Compare<CC::RNE, width>(ectype->GetReference(l), r);
            default:      FATAL("Unreachable"); return false;
        }
    }

    template <Width::Value width> inline bool Cmp(CC cc, FReg l, FReg r)
    {
        return FCmpPrim<width>(cc, l, ectype->GetPrimitive(r));
    }

    template <Width::Value width> inline bool FCmpPrim(CC cc, FReg l, Value::Primitive r)
    {
        switch (cc) {
            case CC::FEQ:  return Compare<CC::FEQ, width>(ectype->GetPrimitive(l), r);
            case CC::FNE:  return Compare<CC::FNE, width>(ectype->GetPrimitive(l), r);
            case CC::FGE:  return Compare<CC::FGE, width>(ectype->GetPrimitive(l), r);
            case CC::FNGE: return Compare<CC::FNGE, width>(ectype->GetPrimitive(l), r);
            case CC::FLT:  return Compare<CC::FLT, width>(ectype->GetPrimitive(l), r);
            case CC::FNLT: return Compare<CC::FNLT, width>(ectype->GetPrimitive(l), r);
            default:       FATAL("Unreachable"); return false;
        }
    }

    template <RT::ImmKind::Value immKind, Width::Value width>
    inline int64_t Bcc(CC cc, Reg l, Reg r, uint16_t offsetValue)
    {
        if (cc.IsFloatingPoint()) {
            return Cmp<width>(cc, l.FR(), r.FR()) ? DecodeImmediate<immKind>(literals, offsetValue) : 0;
        } else {
            return Cmp<width>(cc, l.IR(), r.IR()) ? DecodeImmediate<immKind>(literals, offsetValue) : 0;
        }
    }

    template <RT::ImmKind::Value immValueKind, RT::ImmKind::Value immOffsetKind, Width::Value width>
    inline int64_t BccImm(CC cc, IReg l, uint16_t r, uint16_t offsetValue)
    {
        uint64_t rValue = DecodeImmediate<immValueKind>(literals, r);
        return CmpPrim<width>(cc, l, Value::Primitive { .u64 = rValue })
                   ? DecodeImmediate<immOffsetKind>(literals, offsetValue)
                   : 0;
    }

    inline int64_t Jmp(uint32_t offsetValue) { return static_cast<int32_t>(offsetValue); }

    inline void ExtRet() {}

    inline uint64_t MemOffset(uint64_t offset) { return offset; }

    inline uint64_t MemOffsetReg(IReg reg) { return ectype->GetPrimitive(reg).u64; }

    template <Width::Value width> inline void SCC(CC cc, IReg d, IReg l, IReg r)
    {
        uint64_t res = Cmp<width>(cc, l, r) ? 1 : 0;
        ectype->Put(d, Value::Primitive { .u64 = res });
    }

    template <Width::Value width> inline void SCC(CC cc, IReg d, FReg l, FReg r)
    {
        uint64_t res = Cmp<width>(cc, l, r) ? 1 : 0;
        ectype->Put(d, Value::Primitive { .u64 = res });
    }

    template <RT::ImmKind::Value immKind, Width::Value width> inline void SCCImm(CC cc, IReg d, IReg l, uint16_t imm)
    {
        uint64_t res =
            CmpPrim<width>(cc, l, Value::Primitive { .u64 = DecodeImmediate<immKind>(literals, imm) }) ? 1 : 0;
        ectype->Put(d, Value::Primitive { .u64 = res });
    }

    void Convert(ConvertType toType, ConvertType fromType, Reg to, Reg from)
    {
        auto val = fromType.IsFloatingPoint() ? ectype->GetPrimitive(from.FR()) : ectype->GetPrimitive(from.IR());
        Value::Primitive res = { .u64 = 0 };

        switch (fromType) {
            case ConvertType::I32: {
                auto i32 = static_cast<int32_t>(val.u32);
                switch (toType) {
                    case ConvertType::I8:  res.u64 = (uint64_t)(int8_t)i32; break;
                    case ConvertType::U8:  res.u64 = (uint8_t)i32; break;
                    case ConvertType::I16: res.u64 = (uint64_t)(int16_t)i32; break;
                    case ConvertType::U16: res.u64 = (uint16_t)i32; break;
                    case ConvertType::I64: res.u64 = (uint64_t)(int64_t)i32; break;
                    case ConvertType::F32: res.f32 = (float)i32; break;
                    case ConvertType::F64: res.f64 = (double)i32; break;

                    default: {
                        ASSERTION(false, "unsupported cast");
                    }
                }
                break;
            }

            case ConvertType::U32: {
                auto u32 = val.u32;
                switch (toType) {
                    case ConvertType::I8:  res.u64 = (uint64_t)(int8_t)u32; break;
                    case ConvertType::U8:  res.u64 = (uint8_t)u32; break;
                    case ConvertType::I16: res.u64 = (uint64_t)(int16_t)u32; break;
                    case ConvertType::U16: res.u64 = (uint16_t)u32; break;
                    case ConvertType::I64: res.u64 = (uint64_t)(int64_t)u32; break;
                    case ConvertType::F32: res.f32 = (float)u32; break;
                    case ConvertType::F64: res.f64 = (double)u32; break;

                    default: {
                        ASSERTION(false, "unsupported cast");
                    }
                }
                break;
            }

            case ConvertType::I64: {
                auto i64 = static_cast<int64_t>(val.u64);
                switch (toType) {
                    case ConvertType::I32: res.u64 = (uint64_t)(int32_t)i64; break;
                    case ConvertType::F32: res.f32 = (float)i64; break;
                    case ConvertType::F64: res.f64 = (double)i64; break;

                    default: {
                        ASSERTION(false, "unsupported cast");
                    }
                }
                break;
            }
            case ConvertType::U64: {
                auto u64 = val.u64;
                switch (toType) {
                    case ConvertType::I32: res.u64 = (uint64_t)(int32_t)u64; break;
                    case ConvertType::U32: res.u64 = (uint32_t)u64; break;
                    case ConvertType::F32: res.f32 = (float)u64; break;
                    case ConvertType::F64: res.f64 = (double)u64; break;

                    default: {
                        ASSERTION(false, "unsupported cast");
                    }
                }
                break;
            }
            case ConvertType::F16: {
                ASSERTION(false, "halfs not supported yet");
            }
            case ConvertType::F32: {
                auto f32 = val.f32;
                switch (toType) {
                    case ConvertType::I32: res.u64 = (uint64_t)(int32_t)f32; break;
                    case ConvertType::U32: res.u64 = (uint32_t)f32; break;
                    case ConvertType::I64: res.u64 = (uint64_t)(int64_t)f32; break;
                    case ConvertType::U64: res.u64 = (uint64_t)f32; break;
                    case ConvertType::F16: ASSERTION(false, "halfs not supported yet"); break;
                    case ConvertType::F64: res.f64 = (double)f32; break;

                    default: {
                        ASSERTION(false, "unsupported cast");
                    }
                }
                break;
            }
            case ConvertType::F64: {
                auto f64 = val.f64;
                switch (toType) {
                    case ConvertType::I32: res.u64 = (uint64_t)(int32_t)f64; break;
                    case ConvertType::U32: res.u64 = (uint32_t)f64; break;
                    case ConvertType::I64: res.u64 = (uint64_t)(int64_t)f64; break;
                    case ConvertType::U64: res.u64 = (uint64_t)f64; break;
                    case ConvertType::F32: res.f32 = (float)f64; break;

                    default: {
                        ASSERTION(false, "unsupported cast");
                    }
                }
                break;
            }

            default: {
                ASSERTION(false, "unsupported cast");
            }
        }

        if (toType.IsFloatingPoint()) {
            ectype->Put(to.FR(), res);
        } else {
            ectype->Put(to.IR(), res);
        }
    }

    inline bool BitFieldExtract(Reg dst, Reg src, uint8_t offset, uint8_t size, bool sx)
    {
        auto val = ectype->GetPrimitive(src.IR());

        auto bits = size > 0 ? MathUtils::Bits(val.u64, offset, offset + size - 1) : 0;
        auto res = sx ? MathUtils::SignExtend(bits, size) : bits;

        ectype->Put(dst.IR(), Value::Primitive { .u64 = res });
        return true;
    }

private:
    inline bool NullCheck(Value::Reference obj) { return true; }

    Ectype* ectype;
    Frame frame;
    RTSupport::ThreadHandle handle;
    LiteralTable* literals;
};

} // namespace Interpretation
