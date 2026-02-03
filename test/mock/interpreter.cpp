#include <cstring>

#include "cbc/decoder.h"
#include "cbc/dispatcher_rt.h"
#include "interpreter/interpreter.h"
#include "interpreter.h"
#include "../testutils.h"

namespace Interpretation {

static constexpr int HEAP_SIZE = 4096;
static LimitedHeap<HEAP_SIZE> heap;

struct Test {};

template <>
class RuntimeInterface<Test> {
    using Reference = Value::Reference;
public:

    static TestTypeInfo* Extract(TypeInfo<Test> type) {
        void* p = type;
        return (TestTypeInfo*)p;
    }

    inline static Reference NewObj(TypeInfo<Test> type, ThreadHandle th) {
        auto typeInfo = Extract(type);
        auto mem = heap.do_allocate(typeInfo->size, 16);
        memset(mem, 0, typeInfo->size);
        TestTypeInfo** header = (TestTypeInfo**) mem;
        *header = typeInfo;

        return Value::Reference{.value = reinterpret_cast<uintptr_t>(mem) };
    }

    static Reference ReadObjectInstance(Reference base, size_t offset, ThreadHandle th) {
        return Reference{ .value = *reinterpret_cast<uintptr_t*>(base.value + offset) };
    }

    static void WriteObjectInstance(Reference base, size_t offset, Reference object, ThreadHandle th) {
        *reinterpret_cast<uintptr_t*>(base.value + offset) = object.value;
    }
};

Value::Primitive Interpret(Code code, Value::Primitive ir1, Value::Primitive ir2) {
    heap.Reset();

    Interpretation::Ectype ectype{};
    Interpreter<Test> interp(&ectype, nullptr, nullptr, code.literals);
    Decoder::ByteReader s(code.bytecode, code.bytecode, code.bytecode + code.bytecodeSize);
    ectype.Put(IReg::IR1, ir1);
    ectype.Put(IReg::IR2, ir2);

    Cbc::RT::InterpretationLoop(interp, s);
    //Entry(interp, ctx, s);

    return ectype.GetPrimitive(IReg::IR1);
}

} // namespace Interpretation


Interpretation::Value::Primitive Interpret(
        Interpretation::Code code,
        Interpretation::Value::Primitive ir1,
        Interpretation::Value::Primitive ir2)
{
    return Interpretation::Interpret(code, ir1, ir2);
}
