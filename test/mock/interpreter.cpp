#include <cstring>

#include "../testutils.h"
#include "cbc/decoder.h"
#include "cbc/dispatcher_rt.h"
#include "interpreter.h"
#include "interpreter/adapters.h"

namespace Interpretation {

static constexpr int HEAP_SIZE = 16384;
static LimitedHeap<HEAP_SIZE> heap;

struct Test {};

template <> class RuntimeInterface<Test> {
    using Reference = Value::Reference;

public:
    inline static void* AllocateObject[IReg::COUNT];

    static Reference ReadObjectInstance(Reference base, size_t offset, ThreadHandle th)
    {
        return Reference { .value = *reinterpret_cast<uintptr_t*>(base.value + offset) };
    }

    static void WriteObjectInstance(Reference base, size_t offset, Reference object, ThreadHandle th)
    {
        *reinterpret_cast<uintptr_t*>(base.value + offset) = object.value;
    }

    static Reference ReadObject(uintptr_t base, size_t offset, ThreadHandle th)
    {
        return Reference { .value = *reinterpret_cast<uintptr_t*>(base + offset) };
    }

    static void WriteObject(uintptr_t base, size_t offset, Reference object, ThreadHandle th)
    {
        *reinterpret_cast<uintptr_t*>(base + offset) = object.value;
    }
};

template <uint32_t N> static void MockNewObj(Ectype* ectype, ThreadHandle th, TypeInfo<Test> type);
static void InterpreterI2CallTest(Ectype* ectype, ThreadHandle handle, FunctionHandle* fuh);

static TestTypeInfo* Extract(TypeInfo<Test> type)
{
    void* p = type;
    return (TestTypeInfo*)p;
}

template <uint32_t N> static void MockNewObj(Ectype* ectype, ThreadHandle th, TypeInfo<Test> type)
{
    auto typeInfo = Extract(type);
    auto mem      = heap.do_allocate(typeInfo->size, 16);
    memset(mem, 0, typeInfo->size);
    TestTypeInfo** header = (TestTypeInfo**)mem;
    *header               = typeInfo;

    ectype->Put(IReg::From(N), Value::Reference { .value = reinterpret_cast<uintptr_t>(mem) });
}

static void InterpretationLoop(
    Ectype* ectype, Frame* frame, ThreadHandle th, LiteralTable* literals, Decoder::ByteReader& reader
)
{
    while (true) {
        auto thunk = Cbc::RT::InterpretationLoop<Test>(ectype, frame, th, literals, reader);
        if (!thunk.function) {
            break;
        }
        auto func = reinterpret_cast<void (*)(Ectype*, ThreadHandle, void*)>(thunk.function);
        func(ectype, th, thunk.arg);
    }
}

template <typename RegType>
Value::Primitive Interpret(
    Code code,
    Frame* frame,
    Value::Primitive ir1,
    Value::Primitive ir2,
    Value::Primitive fr0,
    Value::Primitive fr1,
    RegType resReg
)
{
    heap.Reset();

    Interpretation::Ectype ectype {};
    Decoder::ByteReader s(code.bytecode, code.bytecode, code.bytecode + code.bytecodeSize);
    ectype.Put(IReg::IR1, ir1);
    ectype.Put(IReg::IR2, ir2);
    ectype.Put(FReg::FR0, fr0);
    ectype.Put(FReg::FR1, fr1);

    InterpretationLoop(&ectype, frame, nullptr, code.literals, s);

    return ectype.GetPrimitive(resReg);
}

static void InterpreterI2CallTest(Ectype* ectype, ThreadHandle handle, FunctionHandle* fuh)
{
    auto dynFuh   = reinterpret_cast<DynamicFunctionHandle*>(fuh);
    auto bytecode = dynFuh->bytecode.load();
    if (!bytecode) {
        Engine::Session session(Engine::GetEngineInstance());
        auto& manager = FunctionHandleManager::Of(session);
        bytecode      = manager.Prepare(session, dynFuh);
    }
    auto code = bytecode->code;

    Decoder::ByteReader s(code.bytecode, code.bytecode, code.bytecode + code.bytecodeSize);
    InterpretationLoop(ectype, nullptr, handle, code.literals, s);
}

} // namespace Interpretation

Interpretation::Value::Primitive Interpret(
    Interpretation::Code code, Interpretation::Value::Primitive ir1, Interpretation::Value::Primitive ir2
)
{
    return Interpretation::Interpret<Cbc::IReg>(code, nullptr, ir1, ir2, F32(0), F32(0), Cbc::IReg::IR1);
}

Interpretation::Value::Primitive InterpretFPRes(
    Interpretation::Code code, Interpretation::Value::Primitive fr0, Interpretation::Value::Primitive fr1
)
{
    return Interpretation::Interpret<Cbc::FReg>(code, nullptr, U32(0), U32(0), fr0, fr1, Cbc::FReg::FR0);
}

Interpretation::Value::Primitive Interpret(
    Interpretation::Code code,
    Interpretation::Frame* frame,
    Interpretation::Value::Primitive ir1,
    Interpretation::Value::Primitive ir2
)
{
    return Interpretation::Interpret<Cbc::IReg>(code, frame, ir1, ir2, F32(0), F32(0), Cbc::IReg::IR1);
}

Interpretation::Value::Primitive InterpretFPRes(
    Interpretation::Code code,
    Interpretation::Frame* frame,
    Interpretation::Value::Primitive fr0,
    Interpretation::Value::Primitive fr1
)
{
    return Interpretation::Interpret<Cbc::FReg>(code, frame, U32(0), U32(0), fr0, fr1, Cbc::FReg::FR0);
}

void InitializeMockInterpreter()
{
    using namespace Interpretation;
    auto i2call = reinterpret_cast<Interpretation::I2Call>(&Interpretation::InterpreterI2CallTest);
    SetI2CallForInterpreter(i2call);
    RuntimeInterface<Test>::AllocateObject[0]  = reinterpret_cast<void*>(&MockNewObj<0>);
    RuntimeInterface<Test>::AllocateObject[1]  = reinterpret_cast<void*>(&MockNewObj<1>);
    RuntimeInterface<Test>::AllocateObject[2]  = reinterpret_cast<void*>(&MockNewObj<2>);
    RuntimeInterface<Test>::AllocateObject[3]  = reinterpret_cast<void*>(&MockNewObj<3>);
    RuntimeInterface<Test>::AllocateObject[4]  = reinterpret_cast<void*>(&MockNewObj<4>);
    RuntimeInterface<Test>::AllocateObject[5]  = reinterpret_cast<void*>(&MockNewObj<5>);
    RuntimeInterface<Test>::AllocateObject[6]  = reinterpret_cast<void*>(&MockNewObj<6>);
    RuntimeInterface<Test>::AllocateObject[7]  = reinterpret_cast<void*>(&MockNewObj<7>);
    RuntimeInterface<Test>::AllocateObject[8]  = reinterpret_cast<void*>(&MockNewObj<8>);
    RuntimeInterface<Test>::AllocateObject[9]  = reinterpret_cast<void*>(&MockNewObj<9>);
    RuntimeInterface<Test>::AllocateObject[10] = reinterpret_cast<void*>(&MockNewObj<10>);
    RuntimeInterface<Test>::AllocateObject[11] = reinterpret_cast<void*>(&MockNewObj<11>);
    RuntimeInterface<Test>::AllocateObject[12] = reinterpret_cast<void*>(&MockNewObj<12>);
    RuntimeInterface<Test>::AllocateObject[13] = reinterpret_cast<void*>(&MockNewObj<13>);
    static_assert(IReg::COUNT == 14);
}
