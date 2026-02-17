#ifndef INTERPRETER_INTERPRETER_H
#define INTERPRETER_INTERPRETER_H

#include "cbc/isa.h"
#include "ectype.h"
#include "frame.h"
#include "literals.h"
#include "runtime.h"

#include "internal/operations.h"
#include "cbc/isa_rt.h"

namespace Interpretation {

template <typename RTI>
class Interpreter {
    using IReg = Cbc::IReg;

public:
    Interpreter(Ectype* _ectype, Frame* _frame, ThreadHandle _handle, LiteralTable* _literals)
        : ectype(_ectype), frame(_frame), handle(_handle), literals(_literals) {}

    inline void StorePos(uint8_t *c) { /* no-op */ }

    template <Width::Value width>
    inline bool Binary(Common::Value arithOp, IReg d, IReg l, IReg r) {
        return Binary<width>(arithOp, d, l, ectype->GetPrimitive(r));
    }

    template <ImmKind::Value immKind, Width::Value width>
    inline bool BinaryImm(Common::Value arithOp, IReg d, IReg l, uint16_t imm) {
        return Binary<width>(arithOp, d, l, Value::Primitive{ .u64 = DecodeImmediate<immKind>(literals, imm) });
    }

    template <Width::Value width>
    inline bool Binary(Common::Value arithOp, IReg d, IReg l, Value::Primitive val) {
        auto res = Arith<width>(arithOp, ectype->GetPrimitive(l), val);
        if (res.successful) {
            ectype->Put(d, res.result);
            return true;
        }
        return false;
    }

    template <Width::Value width>
    inline bool Binary(FloatOperations::Value fpOp, FReg d, FReg l, FReg r) {
        auto res = ArithFP<width>(fpOp, ectype->GetPrimitive(l), ectype->GetPrimitive(r));
        if (res.successful) {
            ectype->Put(d, res.result);
            return true;
        }
        return false;
    }

    inline bool LoadObj(Format::LoadAccessKind ldk, Format::Reg dst, IReg base, uint64_t offset) {
        auto obj = ectype->GetReference(base);
        if (!NullCheck(obj)) {
            return false;
        }
        if (ldk == LoadAccessKind::LD_REF) {
            ectype->Put(dst.IR(), RuntimeInterface<RTI>::ReadObjectInstance(obj, offset, handle));
        } else {
            MemoryLocation<RTI>(obj.value, offset).LoadPrim(ldk, dst, ectype);
        }
        return true;
    }

    inline bool StoreObj(Format::StoreAccessKind stk, Format::Reg src, IReg base, uint64_t offset) {
        auto obj = ectype->GetReference(base);
        if (!NullCheck(obj)) {
            return false;
        }
        if (stk == StoreAccessKind::ST_REF) {
            RuntimeInterface<RTI>::WriteObjectInstance(obj, offset, ectype->GetReference(src.IR()), handle);
        } else {
            MemoryLocation<RTI>(obj.value, offset).StorePrim(stk, src, ectype);
        }
        return true;
    }

    inline bool LoadRec(Format::LoadAccessKind ldk, Format::Reg dst, IReg base, size_t offset) {
        auto ptr = static_cast<uintptr_t>(ectype->GetPrimitive(base).u64);
        if (ptr == 0) {
            return false;
        }
        if (ldk == LoadAccessKind::LD_REF) {
            auto ref = Value::Reference { .value = *reinterpret_cast<uintptr_t*> (base + offset) };
            ectype->Put(dst.IR(), ref);
        } else {
            MemoryLocation<RTI>(ptr, offset).LoadPrim(ldk, dst, ectype);
        }
        return true;
    }

    inline bool StoreRec(Format::StoreAccessKind stk, Format::Reg src, IReg base, uint64_t offset) {
        auto ptr = static_cast<uintptr_t>(ectype->GetPrimitive(base).u64);
        if (ptr == 0) {
            return false;
        }
        if (stk == StoreAccessKind::ST_REF) {
            auto ref = ectype->GetReference(src.IR()).value;
            *reinterpret_cast<uintptr_t*>(base  + offset) = ref;
        } else {
            MemoryLocation<RTI>(ptr, offset).StorePrim(stk, src, ectype);
        }
        return true;
    }

    inline bool LoadFrame(Format::LoadAccessKind ldk, Format::Reg dst, size_t offset) {
        auto ptr = frame->start;
        if (ptr == 0) {
            return false;
        }
        if (ldk == LoadAccessKind::LD_REF) {
            ectype->Put(dst.IR(), RuntimeInterface<RTI>::ReadObject(ptr, offset, handle));
        } else {
            MemoryLocation<RTI>(ptr, offset).LoadPrim(ldk, dst, ectype);
        }
        return true;
    }

    inline bool StoreFrame(Format::StoreAccessKind stk, Format::Reg src, uint64_t offset) {
        auto ptr = frame->start;
        if (ptr == 0) {
            return false;
        }
        if (stk == StoreAccessKind::ST_REF) {
            RuntimeInterface<RTI>::WriteObject(ptr, offset, ectype->GetReference(src.IR()), handle);
        } else {
            MemoryLocation<RTI>(ptr, offset).StorePrim(stk, src, ectype);
        }
        return true;
    }

    inline bool NewObj(IReg d, uint16_t imm) {
        TypeInfo<RTI> type = literals->at(imm).uintptr;
        auto obj = RuntimeInterface<RTI>::NewObj(type, handle);
        ectype->Put(d, Value::Reference{obj});
        return true;
    }

    inline void MovRef(IReg d, IReg s) {
        ectype->Put(d, ectype->GetReference(s));
    }

    template <typename ToType, typename FromType>
    inline void Mov(ToType d, FromType s) {
        ectype->Put(d, ectype->GetPrimitive(s));
    }

    inline void MovI(IReg d, uint64_t imm) {
        ectype->Put(d, Value::Primitive{ .u64 = imm });
    }

    inline void MovI(FReg d, float imm) {
        ectype->Put(d, Value::Primitive{ .f32 = imm });
    }

    inline void MovI(FReg d, double imm) {
        ectype->Put(d, Value::Primitive{ .f64 = imm });
    }

    template <Width::Value width>
    inline bool Cmp(CC cc, IReg l, IReg r) {
        if (cc.isRef()) {
            return CmpRef<width>(cc, l, ectype->GetReference(r));
        } else {
            return CmpPrim<width>(cc, l, ectype->GetPrimitive(r));
        }
    }

    template <Width::Value width>
    inline bool CmpPrim(CC cc, IReg l, Value::Primitive r) {
        switch (cc) {
            case CC::EQ    : return Compare<CC::EQ, width>(ectype->GetPrimitive(l), r);
            case CC::NE    : return Compare<CC::NE, width>(ectype->GetPrimitive(l), r);
            case CC::LT    : return Compare<CC::LT, width>(ectype->GetPrimitive(l), r);
            case CC::GE    : return Compare<CC::GE, width>(ectype->GetPrimitive(l), r);
            case CC::ULT   : return Compare<CC::ULT, width>(ectype->GetPrimitive(l), r);
            case CC::UGE   : return Compare<CC::UGE, width>(ectype->GetPrimitive(l), r);
            case CC::TESTZ : return Compare<CC::TESTZ, width>(ectype->GetPrimitive(l), r);
            case CC::TESTNZ: return Compare<CC::TESTNZ, width>(ectype->GetPrimitive(l), r);
            default: ASSERTION(false, "Unreachable"); return false;
        }
    }

    template <Width::Value width>
    inline bool CmpRef(CC cc, IReg l, Value::Reference r) {
        switch (cc) {
            case CC::REQ   : return Compare<CC::REQ, width>(ectype->GetReference(l), r);
            case CC::RNE   : return Compare<CC::RNE, width>(ectype->GetReference(l), r);
            default: ASSERTION(false, "Unreachable"); return false;
        }
    }

    template <ImmKind::Value immKind, Width::Value width>
    inline int64_t Bcc(CC cc, IReg l, IReg r, uint16_t offsetValue) {
        return Cmp<width>(cc, l, r) ? DecodeImmediate<immKind>(literals, offsetValue) : 0;
    }

    template <ImmKind::Value immValueKind, ImmKind::Value immOffsetKind, Width::Value width>
    inline int64_t BccImm(CC cc, IReg l, uint16_t r, uint16_t offsetValue) {
        uint64_t rValue = DecodeImmediate<immValueKind>(literals, r);
        return CmpPrim<width>(cc, l, Value::Primitive{ .u64 = rValue }) ? DecodeImmediate<immOffsetKind>(literals, offsetValue) : 0;
    }

    inline int64_t Jmp(uint32_t offsetValue) {
        return static_cast<int32_t>(offsetValue);
    }

    inline void ExtRet() { }

    inline uint64_t MemOffset(uint64_t offset) {
        return offset;
    }

    inline uint64_t MemOffsetReg(IReg reg) {
        return ectype->GetPrimitive(reg).u64;
    }

private:
    inline bool NullCheck(Value::Reference obj) {
        return true;
    }

    Ectype* ectype;
    Frame* frame;
    ThreadHandle handle;
    LiteralTable* literals;
};

} // namespace Interpretation
#endif // INTERPRETER_INTERPRETER_H
