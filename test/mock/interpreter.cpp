#include <cstring>

#include "../testutils.h"
#include "cbc/decoder.h"
#include "cbc/dispatcher_rt.h"
#include "cbc/frame.h"
#include "interpreter.h"
#include "interpreter/adapters.h"
#include "runtime.h"

static constexpr int HEAP_SIZE = 16384;
static LimitedHeap<HEAP_SIZE> heap;

namespace Interpretation {

using namespace RTSupport;

static Interpretation::Frame zeroFrame { reinterpret_cast<uintptr_t>(nullptr) };

static void MockNewObj(Ectype* ectype, ThreadHandle th, TypeInfo<Test> type);
static void InterpreterI2CallTest(Ectype* ectype, ThreadHandle handle, FunctionHandle* fuh);

static TestTypeInfo* Extract(TypeInfo<Test> type)
{
    void* p = type;
    return (TestTypeInfo*)p;
}

static void MockNewObj(Ectype* ectype, ThreadHandle th, TypeInfo<Test> type)
{
    auto typeInfo = Extract(type);
    auto mem      = heap.Allocate(typeInfo->size, alignof(std::max_align_t));
    memset(mem, 0, typeInfo->size);
    TestTypeInfo** header = (TestTypeInfo**)mem;
    *header               = typeInfo;

    auto dst = ectype->GetPrimitive(IReg::IR1);
    ectype->Put(IReg::From(dst.u32), Value::Reference { .value = reinterpret_cast<uintptr_t>(mem) });
}

static void InterpretationLoop(
    Ectype* ectype, Frame frame, ThreadHandle th, LiteralTable* literals, Decoder::ByteReader& reader
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
    Frame frame,
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

    constexpr auto stackSlotCount = 100;
    uint64_t frameSlots[stackSlotCount];
    ASSERTION(bytecode->frameSize / Cbc::STACK_SLOT_SIZE < stackSlotCount, "Frame is too big");
    auto frameStart = reinterpret_cast<uintptr_t>(&frameSlots);
    Interpretation::Frame frame { frameStart };

    Decoder::ByteReader s(code.bytecode, code.bytecode, code.bytecode + code.bytecodeSize);
    InterpretationLoop(ectype, frame, handle, code.literals, s);
}

} // namespace Interpretation

Interpretation::Value::Primitive Interpret(
    Interpretation::Code code, Interpretation::Value::Primitive ir1, Interpretation::Value::Primitive ir2
)
{
    return Interpretation::Interpret<Cbc::IReg>(
        code, Interpretation::zeroFrame, ir1, ir2, F32(0), F32(0), Cbc::IReg::IR1
    );
}

Interpretation::Value::Primitive InterpretFPRes(
    Interpretation::Code code, Interpretation::Value::Primitive fr0, Interpretation::Value::Primitive fr1
)
{
    return Interpretation::Interpret<Cbc::FReg>(
        code, Interpretation::zeroFrame, U32(0), U32(0), fr0, fr1, Cbc::FReg::FR0
    );
}

Interpretation::Value::Primitive Interpret(
    Interpretation::Code code,
    Interpretation::Value::Primitive ir1,
    Interpretation::Value::Primitive ir2,
    Interpretation::Value::Primitive fr0,
    Interpretation::Value::Primitive fr1
)
{
    return Interpretation::Interpret<Cbc::IReg>(code, Interpretation::zeroFrame, ir1, ir2, fr0, fr1, Cbc::IReg::IR1);
}

Interpretation::Value::Primitive InterpretFPRes(
    Interpretation::Code code,
    Interpretation::Value::Primitive ir1,
    Interpretation::Value::Primitive ir2,
    Interpretation::Value::Primitive fr0,
    Interpretation::Value::Primitive fr1
)
{
    return Interpretation::Interpret<Cbc::FReg>(code, Interpretation::zeroFrame, ir1, ir2, fr0, fr1, Cbc::FReg::FR0);
}

Interpretation::Value::Primitive Interpret(
    Interpretation::Code code,
    Interpretation::Frame frame,
    Interpretation::Value::Primitive ir1,
    Interpretation::Value::Primitive ir2
)
{
    return Interpretation::Interpret<Cbc::IReg>(code, frame, ir1, ir2, F32(0), F32(0), Cbc::IReg::IR1);
}

Interpretation::Value::Primitive InterpretFPRes(
    Interpretation::Code code,
    Interpretation::Frame frame,
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
    RuntimeInterface<RTSupport::Test>::AllocateObject = reinterpret_cast<void*>(&MockNewObj);
    static_assert(IReg::COUNT == 14);
}
