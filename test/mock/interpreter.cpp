#include <cstring>

#include "../testutils.h"
#include "cbc/decoder.h"
#include "cbc/frame.h"
#include "engine/typeinfo_manager.h"
#include "interpreter.h"
#include "interpreter/interpretation_loop.h"
#include "runtimesupport/adapters.h"
#include "runtimesupport/runtime.h"
#include "utils/assertion.h"

static constexpr int HEAP_SIZE = 16384;
static LimitedHeap<HEAP_SIZE> heap;

namespace Interpretation {

using namespace RTSupport;

static void MockNewObj(Ectype* ectype, ThreadHandle th, TypeInfo type);
static Interpretation::Frame zeroFrame { 0 };

static void InterpreterI2CallTest(Ectype* ectype, ThreadHandle handle, FunctionHandle* fuh);

static TestTypeInfo* Extract(TypeInfo type)
{
    void* p = type.Raw();
    return (TestTypeInfo*)p;
}

static void MockNewObj(Ectype* ectype, ThreadHandle th, TypeInfo type)
{
    auto typeInfo = Extract(type);
    auto mem      = heap.Allocate(typeInfo->size, alignof(std::max_align_t));
    memset(mem, 0, typeInfo->size);
    TestTypeInfo** header = (TestTypeInfo**)mem;
    *header               = typeInfo;

    auto dst = ectype->GetPrimitive(IReg::IR1);
    ectype->Put(IReg::From(dst.u32), Value::Reference { .value = reinterpret_cast<uintptr_t>(mem) });
}

static void DoInterpretationLoop(
    Ectype* ectype, Frame frame, ThreadHandle th, LiteralTable* literals, Decoder::ByteReader& reader
)
{
    while (true) {
        auto thunk = InterpretationLoop(ectype, frame, th, literals, reader);
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

    DoInterpretationLoop(&ectype, frame, ThreadHandle(nullptr), code.literals, s);

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
    DoInterpretationLoop(ectype, frame, handle, code.literals, s);
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
    static_assert(IReg::COUNT == 14);
}

namespace RTSupport {

using Reference = Interpretation::Value::Reference;

std::optional<TypeInfo> CreateTypeInfo(
    Engine::Session& session, Engine::TypeInfoManager& manager, Symlevel::GlobalTerm term
)
{
    return std::nullopt;
}

Reference Execution::ReadObjectInstance(Reference base, size_t offset, ThreadHandle th)
{
    return Reference { .value = *reinterpret_cast<uintptr_t*>(base.value + offset) };
}

void Execution::WriteObjectInstance(Reference base, size_t offset, Reference object, ThreadHandle th)
{
    *reinterpret_cast<uintptr_t*>(base.value + offset) = object.value;
}

Reference Execution::ReadObjectStatic(void* location, ThreadHandle th)
{
    return Reference { .value = *reinterpret_cast<uintptr_t*>(location) };
}

void Execution::WriteObjectStatic(void* location, Reference object, ThreadHandle th)
{
    *reinterpret_cast<uintptr_t*>(location) = object.value;
}

int Execution::GetFieldOffset(TypeInfo ti, int ordinal) { FATAL("Should not reach here. Get field offset"); }

TypeInfo Execution::GetTypeInfo(Reference base)
{
    TypeInfo* header = reinterpret_cast<TypeInfo*>(base.value);
    return *header;
}

MethodTable Execution::GetMethodTable(Reference base, int extDefNum, int methodNum)
{
    FATAL("Should not reach here. I2C virtual call");
}

void* Execution::AllocateObjectInstance() { return reinterpret_cast<void*>(&Interpretation::MockNewObj); }

void* Adapters::GenericI2CCallInstance() { FATAL("Should not reach here. Mock i2c"); }

void* Adapters::I2ICallInstance() { return reinterpret_cast<void*>(&Interpretation::InterpreterI2CallTest); }

void* Adapters::IregOnlyC2ICallInstance() { FATAL("Should not reach here. Mock c2i"); }

void* Adapters::GetDirectCallTrampoline(Interpretation::DynamicFunctionHandle* fuh) { FATAL("Should not reach here."); }

} // namespace RTSupport
