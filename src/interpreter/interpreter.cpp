#include "cbc/isa.h"
#include "cbc/dispatcher.h"
#include "ectype.h"
#include "frame.h"

using namespace Cbc::Format;

namespace Interpretation {

using ThreadHandle = void*;

struct InterpreterContext {
	ThreadHandle handle;
};


struct Interpreter {
	using Context = InterpreterContext;
	using IReg = Cbc::IReg;
	Ectype* ectype;
	Frame* frame;

	inline void StorePos(uint8_t *c) { /* no-op */ }

	template <Width::Value width, Common::Value arithOp>
	inline void Common(Context ctx, IReg l, IReg r) {
		(void)l;
		(void)r;
	}

	inline void MovRef(Context ctx, IReg l, IReg r) {
		// stub
	}

	inline void MovVST(Context ctx, IReg l, IReg r) {
		// stub
	}

	template <CC::Value cc, Width::Value width>
	inline int32_t B2rrd8BranchIf(Context ctx, IReg l, IReg r) {
		(void)l;
		(void)r;
		return 0;
	}

	template <ImmKind::Value immKind, Width::Value width>
	inline int32_t ExtBcc(Context ctx, CC cc, IReg l, IReg r, uint16_t offsetValue) {
        (void)cc;
		(void)l;
		(void)r;
        (void)offsetValue;
		return 0;
	}
};

// Stub entry-point
template <typename Handler = Interpreter>
void Entry(Handler handler, Interpreter::Context ctx, Decoder::ByteReader stream) {
	using namespace Decoder;
	NEXT;
}

// Stub for template instantioation
void stub() {
	Interpreter i{};
	Decoder::ByteReader s(0,0,0);
	Interpreter::Context ctx;
	Entry(i, ctx, s);
}

} // namespace Interpretation
