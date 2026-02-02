#ifndef INTERPRETER_INTERPRETER_H
#define INTERPRETER_INTERPRETER_H

#include "cbc/isa.h"
#include "ectype.h"
#include "frame.h"
#include "literals.h"
#include "runtime.h"

#include "internal/operations.h"

namespace Interpretation {

template <typename RT>
class Interpreter {
    using IReg = Cbc::IReg;

public:
    Interpreter(Ectype* _ectype, Frame* _frame, ThreadHandle _handle, LiteralTable* _literals)
        : ectype(_ectype), frame(_frame), handle(_handle), literals(_literals) {}

    inline void StorePos(uint8_t *c) { /* no-op */ }

    template <Width::Value width>
    inline bool Binary(Common::Value arithOp, IReg d, IReg l, IReg r) {
        auto res = Arith<width>(arithOp, ectype->GetPrimitive(l), ectype->GetPrimitive(r));
        if (res.successful) {
            ectype->Put(d, res.result);
            return true;
        }
        return false;
    }

    inline bool NewObj(IReg d, uint16_t imm) {
        TypeInfo<RT> type = literals->at(imm).uintptr;
        auto obj = RuntimeInterface<RT>::NewObj(type, handle);
        ectype->Put(d, Value::Reference{obj});
        return true;
    }

    inline void MovRef(IReg d, IReg s) {
        ectype->Put(d, ectype->GetReference(s));
    }

    inline void Mov(IReg d, IReg s) {
        ectype->Put(d, ectype->GetReference(s));
    }

    template <Width::Value width>
    bool Cmp(CC cc, IReg l, IReg r) {
        switch (cc) {
            case CC::EQ    : return Compare<CC::EQ, width>(ectype->GetPrimitive(l), ectype->GetPrimitive(r));
            case CC::NE    : return Compare<CC::NE, width>(ectype->GetPrimitive(l), ectype->GetPrimitive(r));
            case CC::LT    : return Compare<CC::LT, width>(ectype->GetPrimitive(l), ectype->GetPrimitive(r));
            case CC::GE    : return Compare<CC::GE, width>(ectype->GetPrimitive(l), ectype->GetPrimitive(r));
            case CC::ULT   : return Compare<CC::ULT, width>(ectype->GetPrimitive(l), ectype->GetPrimitive(r));
            case CC::UGE   : return Compare<CC::UGE, width>(ectype->GetPrimitive(l), ectype->GetPrimitive(r));
            case CC::REQ   : return Compare<CC::REQ, width>(ectype->GetReference(l), ectype->GetReference(r));
            case CC::RNE   : return Compare<CC::RNE, width>(ectype->GetReference(l), ectype->GetReference(r));
            case CC::TESTZ : return Compare<CC::TESTZ, width>(ectype->GetPrimitive(l), ectype->GetPrimitive(r));
            case CC::TESTNZ: return Compare<CC::TESTNZ, width>(ectype->GetPrimitive(l), ectype->GetPrimitive(r));
            default: ASSERTION(false, "Unreachable"); return false;
        }
    }

    template <ImmKind::Value immKind, Width::Value width>
    inline int32_t Bcc(CC cc, IReg l, IReg r, uint16_t offsetValue) {
        return Cmp<width>(cc, l, r) ? JumpOffset<immKind>(literals, offsetValue) : 0;
    }

    inline void ExtRet() { }
private:
    Ectype* ectype;
    Frame* frame;
    ThreadHandle handle;
    LiteralTable* literals;
};

} // namespace Interpretation
#endif // INTERPRETER_INTERPRETER_H
