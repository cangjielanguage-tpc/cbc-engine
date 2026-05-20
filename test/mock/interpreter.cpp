#include <cstddef>
#include <cstdint>
#include <cstring>

#include "../testutils.h"
#include "cbc/decoder.h"
#include "cbc/frame.h"
#include "cbc/isa.h"
#include "engine/typeinfo_manager.h"
#include "interpreter.h"
#include "interpreter/ectype.h"
#include "interpreter/function_handle.h"
#include "interpreter/interpretation_loop.h"
#include "interpreter/loggers.h"
#include "runtimesupport/adapters.h"
#include "runtimesupport/runtime.h"
#include "utils/assertion.h"
#include "utils/logger.h"

static constexpr int HEAP_SIZE = 16384;
static LimitedHeap<HEAP_SIZE> heap;

namespace Interpretation {

using namespace RTSupport;

static void MockNewObj(Ectype* ectype, ThreadHandle th, TypeInfo type);
static Interpretation::Frame zeroFrame { 0 };

static void InterpreterI2CallTest(Ectype* ectype, ThreadHandle handle, DynamicFunctionHandle* fuh);

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

    ectype->Put(IReg::IR1, Value::Reference { .value = reinterpret_cast<uintptr_t>(mem) });
}

static void DoInterpretationLoop(
    Ectype* ectype, Frame frame, ThreadHandle th, LiteralTable* literals, Decoder::ByteReader& reader
)
{
    Log::interpretation.Stream(Logging::Level::DEBUG).PrintFmt("Started interpration of mock method");
    Log::interpretation.Stream(Logging::Level::DEBUG).NewLine();
    while (true) {
        auto thunk = InterpretationLoop(ectype, frame, th, literals, reader);
        if (!thunk.function) {
            break;
        }
        auto func = reinterpret_cast<void (*)(Ectype*, ThreadHandle, void*)>(thunk.function);
        func(ectype, th, thunk.arg);
    }
    Log::interpretation.Stream(Logging::Level::DEBUG).PrintFmt("Stopped interpration of mock method");
    Log::interpretation.Stream(Logging::Level::DEBUG).NewLine();
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
    uint8_t bufIrs[sizeof(ectype.iregs)];
    uint8_t bufFrs[sizeof(ectype.fregs)];

    uint32_t prng = 1;
    for (int i = 0; i < sizeof(bufIrs); i++) {
        prng = 1664525 * prng + 1013904223;
        bufIrs[i] = (prng >> 16) & 0xff;
    }
    for (int i = 0; i < sizeof(bufFrs); i++) {
        prng = 1664525 * prng + 1013904223;
        bufFrs[i] = (prng >> 16) & 0xff;
    }

    memcpy(&ectype.iregs, bufIrs, sizeof(bufIrs));
    memcpy(&ectype.fregs, bufFrs, sizeof(bufFrs));

    Decoder::ByteReader s(code.bytecode, code.bytecode, code.bytecode + code.bytecodeSize);
    ectype.iregs[0].primitive = Value::Primitive { .u64 = 0 };
    ectype.Put(IReg::IR1, ir1);
    ectype.Put(IReg::IR2, ir2);
    ectype.Put(FReg::FR0, fr0);
    ectype.Put(FReg::FR1, fr1);

    DoInterpretationLoop(&ectype, frame, ThreadHandle(nullptr), code.literals, s);

    return ectype.GetPrimitive(resReg);
}

static void InterpreterI2CallTest(Ectype* ectype, ThreadHandle handle, DynamicFunctionHandle* fuh)
{
    auto bytecode = fuh->bytecode.load();
    if (!bytecode) {
        Engine::Session session(Engine::GetEngineInstance());
        auto& manager = FunctionHandleManager::Of(session);
        bytecode      = manager.Prepare(session, fuh);
    }
    auto code = bytecode->code;

    constexpr auto stackSlotCount = 100;
    uint64_t frameSlots[stackSlotCount];
    ASSERTION(bytecode->frameSize / Cbc::STACK_SLOT_SIZE < stackSlotCount, "Frame is too big");
    auto frameStart = reinterpret_cast<uintptr_t>(&frameSlots);
    Interpretation::Frame frame { frameStart };

    IRegContainer iregs[IReg::COUNT];
    FRegContainer fregs[FReg::COUNT];
    for (auto i = 8; i != IReg::COUNT; i++) { // TODO: first non-vol index is arch-dependent
        iregs[i].primitive = ectype->GetPrimitive(IReg::From(i));
    }
    for (auto i = 8; i != FReg::COUNT; i++) {
        fregs[i].primitive = ectype->GetPrimitive(FReg::From(i));
    }

    Decoder::ByteReader s(code.bytecode, code.bytecode, code.bytecode + code.bytecodeSize);
    InterpretationStart(fuh, ectype);
    DoInterpretationLoop(ectype, frame, handle, code.literals, s);
    InterpretationEnd(fuh, ectype);

    for (auto i = 8; i != IReg::COUNT; i++) {
        ectype->Put(IReg::From(i), iregs[i].primitive);
    }
    for (auto i = 8; i != FReg::COUNT; i++) {
        ectype->Put(FReg::From(i), fregs[i].primitive);
    }
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
    Engine::InitEnvOptions();
    auto i2call = reinterpret_cast<Interpretation::I2Call>(&Interpretation::InterpreterI2CallTest);
}

namespace RTSupport {

using Reference = Interpretation::Value::Reference;

std::optional<TypeInfo> CreateTypeInfo(
    Engine::Session& session, Engine::TypeInfoManager& manager, Engine::GlobalTerm term
)
{
    return std::nullopt;
}

Engine::GlobalTerm ReconstructTerm(Engine::Session& session, Engine::TypeInfoManager& manager, TypeInfo ti)
{
    FATAL("Should not be called");
}

Reference Execution::ReadObjectInstance(Reference base, size_t offset, ThreadHandle th)
{
    return Reference { .value = *reinterpret_cast<uintptr_t*>(base.value + offset) };
}

void Execution::WriteObjectInstance(Reference base, size_t offset, Reference object, ThreadHandle th)
{
    *reinterpret_cast<uintptr_t*>(base.value + offset) = object.value;
}

Reference Execution::ReadArrayElem(Reference array, uint64_t index, ThreadHandle th)
{
    return Reference { .value = *reinterpret_cast<uintptr_t*>(
                           array.value + RTSupport::MetaInfo::ArrayBodyOffset() + index * sizeof(uintptr_t)
                       ) };
}

void Execution::WriteArrayElem(Reference array, uint64_t index, Reference object, ThreadHandle th)
{
    *reinterpret_cast<uintptr_t*>(array.value + RTSupport::MetaInfo::ArrayBodyOffset() + index * sizeof(uintptr_t)) =
        object.value;
}

Reference Execution::ReadObjectStatic(void* location, ThreadHandle th)
{
    return Reference { .value = *reinterpret_cast<uintptr_t*>(location) };
}

void Execution::WriteObjectStatic(void* location, Reference object, ThreadHandle th)
{
    *reinterpret_cast<uintptr_t*>(location) = object.value;
}

uint32_t Execution::GetFieldOffset(TypeInfo ti, int ordinal, bool adjustByHeader)
{
    FATAL("Should not reach here. Get field offset");
}

TypeInfo Execution::GetTypeInfo(Reference base)
{
    TypeInfo* header = reinterpret_cast<TypeInfo*>(base.value);
    return *header;
}

void* Execution::GetVirtualTarget(Reference base, int extDefNum, int methodNum)
{
    FATAL("Should not reach here. I2C virtual call");
}

void* Execution::GetInterfaceTarget(Reference base, TypeInfo ti, int methodNum)
{
    FATAL("Should not reach here. I2C interface call");
}

void* Execution::AllocateObjectInstance() { return reinterpret_cast<void*>(&Interpretation::MockNewObj); }

void* Execution::AllocateObjectInstanceAcc() { return reinterpret_cast<void*>(&Interpretation::MockNewObj); }

void* Execution::AllocateArrayInstance() { return reinterpret_cast<void*>(&Interpretation::MockNewObj); }

void* Execution::GcPointTrampoline() { FATAL("Should not reach here"); }

void* Execution::GcPoint() { FATAL("Should not reach here"); }

void* Execution::Spawn() { FATAL("Should not reach here"); }

bool Execution::IsPendingSafePoint() { return false; }

bool Execution::IsInstanceOf(Reference base, TypeInfo ti) { FATAL("Should not reach here"); }

TypeInfo Execution::LoadTypeInfo(Engine::GlobalTerm term, Interpretation::Ectype* ectype, void* stackSlots)
{
    FATAL("Should not reach here");
}

bool Execution::IsGlobalStruct(Reference base, uintptr_t derived) { FATAL("Should not reach here"); }

Reference Execution::GetGlobalBasePtr() { FATAL("Should not reach here"); }

Reference Execution::GetLocalBasePtr() { FATAL("Should not reach here"); }

void* Adapters::GenericI2CCallInstance() { FATAL("Should not reach here. Mock i2c"); }

void* Adapters::I2ICallInstance() { return reinterpret_cast<void*>(&Interpretation::InterpreterI2CallTest); }

static void C2ICall() { FATAL("Should not reach here. Mock c2i"); }

void* Adapters::GetDynCallTrampoline(int idx) { FATAL("Should not reach here"); }

void* Adapters::GenericC2ICallInstance() { return reinterpret_cast<void*>(&C2ICall); }

void* Adapters::IregOnlyC2ICallInstance() { return reinterpret_cast<void*>(&C2ICall); }

void* Adapters::C2ICall(uint32_t intArgCount, uint32_t floatArgCount) { return reinterpret_cast<void*>(&C2ICall); }

void* Adapters::GetDirectCallTrampoline(Interpretation::DynamicFunctionHandle* fuh) { FATAL("Should not reach here."); }

const char* MetaInfo::GetName(TypeInfo ti) { return "<unknown>"; }

uint32_t MetaInfo::GetTypeSize(TypeInfo ti) { return Interpretation::Extract(ti)->size; }

uint8_t MetaInfo::GetAlign(TypeInfo ti) { return alignof(max_align_t); }

bool MetaInfo::IsReferenceType(TypeInfo ti) { return false; }

void MetaInfo::VisitReferences(TypeInfo ti, std::function<void(uint32_t)> visitor) { FATAL("Should not be called"); }

TypeInfo MetaInfo::ByteArrayTypeInfo() { return TypeInfo(nullptr); }

TypeInfoUUID MetaInfo::GetUUID(TypeInfo ti) { return 0; }

} // namespace RTSupport
