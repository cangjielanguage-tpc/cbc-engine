#include "cbc/isa.h"
#include "cbc/dispatcher.h"
#include "ectype.h"
#include "frame.h"
#include "literals.h"
#include "internal/operations.h"

namespace Interpretation {

using namespace Cbc::Format;
using ThreadHandle = void*;

struct InterpreterContext {
    ThreadHandle handle;
    LiteralTable *literals;
};

struct Interpreter {
    using Context = InterpreterContext;
    using IReg = Cbc::IReg;
    Ectype* ectype;
    Frame* frame;

    inline void StorePos(uint8_t *c) { /* no-op */ }

    template <Width::Value width, Common::Value arithOp>
    inline bool Common(Context ctx, IReg l, IReg r) {
        auto res = Arith<width>(arithOp, ectype->GetPrimitive(l), ectype->GetPrimitive(r));
        if (res.successful) {
            ectype->Put(l, res.result);
            return true;
        }
        return false;
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
};

// Stub entry-point
template <typename Handler = Interpreter>
void Entry(Handler handler, Interpreter::Context ctx, Decoder::ByteReader stream) {
    using namespace Decoder;
    NEXT;
}

// Stub for template instantiation
void stub() {
    Interpreter i{};
    Decoder::ByteReader s(0,0,0);
    Interpreter::Context ctx;
    Entry(i, ctx, s);
}

} // namespace Interpretation
