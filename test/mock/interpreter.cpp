#include <cstring>

#include "cbc/decoder.h"
#include "cbc/dispatcher_rt.h"
#include "interpreter/interpreter.h"
#include "interpreter.h"
#include "../testutils.h"

namespace Interpretation {

static constexpr int HEAP_SIZE = 16384;
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

    static Reference ReadObject(uintptr_t base, size_t offset, ThreadHandle th) {
        return Reference { .value = *reinterpret_cast<uintptr_t*> (base + offset) };
    }

    static void WriteObject(uintptr_t base, size_t offset, Reference object, ThreadHandle th) {
        *reinterpret_cast<uintptr_t*>(base  + offset) = object.value;
    }
};

template <typename RegType>
Value::Primitive Interpret(Code code, Frame* frame, Value::Primitive ir1, Value::Primitive ir2, RegType resReg) {
    heap.Reset();

    Interpretation::Ectype ectype{};
    Interpreter<Test> interp(&ectype, frame, nullptr, code.literals);
    Decoder::ByteReader s(code.bytecode, code.bytecode, code.bytecode + code.bytecodeSize);
    ectype.Put(IReg::IR1, ir1);
    ectype.Put(IReg::IR2, ir2);

    Cbc::RT::InterpretationLoop(interp, s);

    return ectype.GetPrimitive(resReg);
}

} // namespace Interpretation


Interpretation::Value::Primitive Interpret(
        Interpretation::Code code,
        Interpretation::Value::Primitive ir1,
        Interpretation::Value::Primitive ir2)
{
    return Interpretation::Interpret<Cbc::IReg>(code, nullptr, ir1, ir2, Cbc::IReg::IR1);
}

Interpretation::Value::Primitive InterpretFPRes(
        Interpretation::Code code,
        Interpretation::Value::Primitive ir1,
        Interpretation::Value::Primitive ir2)
{
    return Interpretation::Interpret<Cbc::FReg>(code, nullptr, ir1, ir2, Cbc::FReg::FR0);
}

Interpretation::Value::Primitive Interpret(
        Interpretation::Code code,
        Interpretation::Frame* frame,
        Interpretation::Value::Primitive ir1,
        Interpretation::Value::Primitive ir2)
{
    return Interpretation::Interpret<Cbc::IReg>(code, frame, ir1, ir2, Cbc::IReg::IR1);
}

Interpretation::Value::Primitive InterpretFPRes(
        Interpretation::Code code,
        Interpretation::Frame* frame,
        Interpretation::Value::Primitive ir1,
        Interpretation::Value::Primitive ir2)
{
    return Interpretation::Interpret<Cbc::FReg>(code, frame, ir1, ir2, Cbc::FReg::FR0);
}
