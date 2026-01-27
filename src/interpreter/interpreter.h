#ifndef INTERPRETER_INTERPRETER_H
#define INTERPRETER_INTERPRETER_H

#include "cbc/isa.h"
#include "cbc/dispatcher.h"
#include "ectype.h"
#include "frame.h"
#include "literals.h"
#include "runtime.h"

#include "internal/operations.h"

namespace Interpretation {

struct InterpreterContext {
    ThreadHandle handle;
    LiteralTable *literals;
};

template <typename RT>
class Interpreter {
    using IReg = Cbc::IReg;

public:
    using Context = InterpreterContext;

    Interpreter(Ectype* _ectype, Frame* _frame)
        : ectype(_ectype), frame(_frame) {}

    inline void StorePos(uint8_t *c) { /* no-op */ }

    template <Width::Value width, Common::Value arithOp>
    inline bool Common2R(Context ctx, IReg l, IReg r) {
        return Common3R<width, arithOp>(ctx, l, l, r);
    }

    template <Width::Value width, Common::Value arithOp>
    inline bool Common3R(Context ctx, IReg dst, IReg l, IReg r) {
        return Common3I<width, arithOp>(ctx, dst, l, ectype->GetPrimitive(r));
    }

    template <Width::Value width, Common::Value arithOp>
    inline bool Common3I(Context ctx, IReg dst, IReg l, uint64_t imm) {
        return Common3I<width, arithOp>(ctx, dst, l, Value::Primitive{ .u64 = imm });
    }

    template <Width::Value width, Common::Value arithOp>
    inline bool Common3I(Context ctx, IReg dst, IReg l, Value::Primitive val) {
        auto res = Arith<width>(arithOp, ectype->GetPrimitive(l), val);
        if (res.successful) {
            ectype->Put(dst, res.result);
            return true;
        }
        return false;
    }

    template <Width::Value width>
    inline void Mov(Context ctx, IReg d, IReg s) {
        ectype->Put(d, ectype->GetPrimitive(s));
    }

    template <Width::Value width>
    inline void MovI(Context ctx, IReg d, uint64_t imm) {
        ectype->Put(d, Value::Primitive{ .u64 = imm });
    }

    inline bool NewObj(Context ctx, IReg d, uint16_t imm) {
        TypeInfo<RT> type = ctx.literals->at(imm).uintptr;
        auto obj = RuntimeInterface<RT>::NewObj(type, ctx.handle);
        ectype->Put(d, Value::Reference{obj});
        return true;
    }

    inline void MovRef(Context ctx, IReg d, IReg s) {
        ectype->Put(d, ectype->GetReference(s));
    }

    inline void MovVST(Context ctx, IReg l, IReg r) {
        ASSERTION(false, "not implemented");
    }

    template <CC::Value cc, Width::Value width>
    inline int32_t B2rrd8BranchIf(Context ctx, IReg l, IReg r, int8_t offset) {
        ASSERTION(false, "must be rewritten");
    	return 0;
    }

    template <CC::Value cc, ImmKind::Value immKind, Width::Value width>
    inline int32_t ExtBcc(Context ctx, IReg l, IReg r, uint16_t offsetValue) {
        bool shouldJump;
        if constexpr (cc == CC::REQ || cc == CC::RNE) {
            shouldJump = Compare<cc>(ectype->GetReference(l), ectype->GetReference(r));
        } else {
            shouldJump = Compare<cc, width>(ectype->GetPrimitive(l), ectype->GetPrimitive(r));
        }
        return shouldJump ? JumpOffset<immKind>(ctx.literals, offsetValue) : 0;
    }

    inline void ExtRet(Context ctx) { }
private:
    Ectype* ectype;
    Frame* frame;
};

} // namespace Interpretation
#endif // INTERPRETER_INTERPRETER_H
