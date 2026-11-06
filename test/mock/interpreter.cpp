#include <cstddef>
#include <cstdint>
#include <cstring>
#include <unordered_map>

#include "../testutils.h"
#include "cbc/decoder.h"
#include "cbc/frame.h"
#include "cbc/isa.h"
#include "engine/options.h"
#include "engine/typeinfo_manager.h"
#include "interpreter.h"
#include "interpreter/code.h"
#include "interpreter/ectype.h"
#include "interpreter/function_handle.h"
#include "interpreter/interpretation_loop.h"
#include "interpreter/loggers.h"
#include "runtimesupport/adapters.h"
#include "runtimesupport/runtime.h"
#include "runtimesupport/typeinfo_factory.h"
#include "utils/assertion.h"
#include "utils/logger.h"
#include "utils/math.h"

static constexpr int HEAP_SIZE = 16384;
static LimitedHeap<HEAP_SIZE> heap;

namespace Interpretation {

using namespace RTSupport;

static void MockNewObj(Ectype* ectype, ThreadHandle th, TypeInfo type);
static void MockNewArr(Ectype* ectype, ThreadHandle th, TypeInfo elemType);
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

// Array layout must match the runtime constants the rewriter emits against:
// [TypeInfo*][u64 length][elements...] with ObjectHeaderSize 8 and ArrayBodyOffset 16;
// the element stride follows MetaInfo::GetAlignedSize, same as LeaIndex.
static void MockNewArr(Ectype* ectype, ThreadHandle th, TypeInfo elemType)
{
    auto elemSize = MetaInfo::GetAlignedSize(elemType);
    auto count    = ectype->GetPrimitive(IReg::IR2).u64;
    auto size     = MetaInfo::ArrayBodyOffset() + count * elemSize;
    auto mem      = heap.Allocate(size, alignof(std::max_align_t));
    memset(mem, 0, size);
    TestTypeInfo** header = (TestTypeInfo**)mem;
    *header               = Extract(elemType);
    *reinterpret_cast<uint64_t*>(static_cast<uint8_t*>(mem) + MetaInfo::ObjectHeaderSize()) = count;

    ectype->Put(IReg::IR1, Value::Reference { .value = reinterpret_cast<uintptr_t>(mem) });
}

static Type g_lastImplicitThrow = Type::Successful;

// Frame geometry (mirrors the compiled trampoline, perform_2i_call in arch_os/*/trampolines.S):
// frame.start points one past the last frame slot; frame slots are addressed as negative offsets
// from it (slot N at frame.start + N * STACK_SLOT_SIZE - staticFrameSize, where staticFrameSize is
// ExecBytecodeInfo::staticFrameSize). The stack-passed call arguments are staged by the caller at
// frameBot + k * STACK_SLOT_SIZE (st.stack.param*/lea.stack.param, stack-passed call type infos),
// where frameBot = frame.start - frameSize (ExecBytecodeInfo::frameSize) is the lowest address of
// the frame region: frameSize = staticFrameSize + the outgoing param-passing area that the rewriter
// expands below the addressed slots (aligned to FRAME_ALIGNMENT). An I2I callee reads its incoming
// stack params at [IR7 + k * STACK_SLOT_SIZE] — the tail register, which the real asm thunk points
// at the caller's frameBot before entering the callee.
static void DoInterpretationLoop(
    Ectype* ectype, Frame frame, ThreadHandle th, LiteralTable* literals, Decoder::ByteReader& reader, uintptr_t frameBot
)
{
    Log::interpretation.Stream(Logging::Level::DEBUG).PrintFmt("Started interpration of mock method");
    Log::interpretation.Stream(Logging::Level::DEBUG).NewLine();
    while (true) {
        auto thunk = InterpretationLoop(ectype, frame, th, literals, reader, frameBot);
        if (thunk.op == ThunkOp::None) {
            break;
        }
        switch (thunk.op) {
            case ThunkOp::NewObject:
            case ThunkOp::NewObjectAcc: MockNewObj(ectype, th, TypeInfo(thunk.arg)); break;
            case ThunkOp::NewArray: MockNewArr(ectype, th, TypeInfo(thunk.arg)); break;
            case ThunkOp::I2ICall:
                ectype->Put(IReg::IR7, U64(frameBot));
                InterpreterI2CallTest(ectype, th, reinterpret_cast<DynamicFunctionHandle*>(thunk.arg));
                break;
            case ThunkOp::ThrowImplicit:
                g_lastImplicitThrow = static_cast<Type>(reinterpret_cast<uintptr_t>(thunk.arg));
                return;
            default: FATAL("Unhandled mock thunk op");
        }
    }
    Log::interpretation.Stream(Logging::Level::DEBUG).PrintFmt("Stopped interpration of mock method");
    Log::interpretation.Stream(Logging::Level::DEBUG).NewLine();
}

Type LastImplicitThrow() { return g_lastImplicitThrow; }

template <typename RegType>
Value::Primitive Interpret(
    Code code,
    Frame frame,
    Value::Primitive ir1,
    Value::Primitive ir2,
    Value::Primitive fr0,
    Value::Primitive fr1,
    RegType resReg,
    Value::Primitive ir3 = Value::Primitive { .u64 = 0 },
    uintptr_t frameBot = 0
)
{
    heap.Reset();
    g_lastImplicitThrow = Type::Successful;

    Interpretation::Ectype ectype {};
    uint8_t bufIrs[sizeof(ectype.iregs)];
    uint8_t bufFrs[sizeof(ectype.fregs)];

    uint32_t prng = 1;
    for (int i = 0; i < sizeof(bufIrs); i++) {
        prng      = 1664525 * prng + 1013904223;
        bufIrs[i] = (prng >> 16) & 0xff;
    }
    for (int i = 0; i < sizeof(bufFrs); i++) {
        prng      = 1664525 * prng + 1013904223;
        bufFrs[i] = (prng >> 16) & 0xff;
    }

    memcpy(&ectype.iregs, bufIrs, sizeof(bufIrs));
    memcpy(&ectype.fregs, bufFrs, sizeof(bufFrs));

    Decoder::ByteReader s(code.bytecode, code.bytecode, code.bytecode + code.bytecodeSize);
    ectype.iregs[0].primitive = Value::Primitive { .u64 = 0 };
    ectype.Put(IReg::IR1, ir1);
    ectype.Put(IReg::IR2, ir2);
    ectype.Put(IReg::IR3, ir3);
    ectype.Put(FReg::FR0, fr0);
    ectype.Put(FReg::FR1, fr1);

    DoInterpretationLoop(&ectype, frame, ThreadHandle(nullptr), code.literals, s, frameBot);

    return ectype.GetPrimitive(resReg);
}

static void InterpreterI2CallTest(Ectype* ectype, ThreadHandle handle, DynamicFunctionHandle* fuh)
{
    auto bytecode = fuh->bytecode.load();
    if (!bytecode) {
        Engine::Session session(Engine::GetEngineInstance());
        auto& manager = FunctionHandleManager::Of(session);
        bytecode      = manager.Prepare(session, fuh).info;
    }
    auto code = bytecode->code;

    constexpr auto stackSlotCount = 100;
    uint64_t frameSlots[stackSlotCount];
    ASSERTION(bytecode->frameSize / Cbc::STACK_SLOT_SIZE < stackSlotCount, "Frame is too big");
    // Frame slots are addressed negatively relative to the frame start:
    // slot N is located at frame.start + N * STACK_SLOT_SIZE - frameSize.
    auto frameStart = reinterpret_cast<uintptr_t>(frameSlots + stackSlotCount);
    Interpretation::Frame frame { frameStart };
    auto frameBot = frameStart - bytecode->frameSize;

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
    DoInterpretationLoop(ectype, frame, handle, code.literals, s, frameBot);
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
    Interpretation::Value::Primitive ir2,
    uintptr_t frameBot
)
{
    return Interpretation::Interpret<Cbc::IReg>(
        code, frame, ir1, ir2, F32(0), F32(0), Cbc::IReg::IR1, U64(0), frameBot
    );
}

Interpretation::Value::Primitive Interpret(
    Interpretation::Code code,
    Interpretation::Value::Primitive ir1,
    Interpretation::Value::Primitive ir2,
    Interpretation::Value::Primitive ir3
)
{
    return Interpretation::Interpret<Cbc::IReg>(
        code, Interpretation::zeroFrame, ir1, ir2, F32(0), F32(0), Cbc::IReg::IR1, ir3
    );
}

Interpretation::Type InterpretThrows(
    Interpretation::Code code, Interpretation::Value::Primitive ir1, Interpretation::Value::Primitive ir2
)
{
    return InterpretThrows(code, ir1, ir2, U32(0));
}

Interpretation::Type InterpretThrows(
    Interpretation::Code code,
    Interpretation::Value::Primitive ir1,
    Interpretation::Value::Primitive ir2,
    Interpretation::Value::Primitive ir3
)
{
    Interpretation::Interpret<Cbc::IReg>(
        code, Interpretation::zeroFrame, ir1, ir2, F32(0), F32(0), Cbc::IReg::IR1, ir3
    );
    return Interpretation::LastImplicitThrow();
}

Interpretation::Value::Primitive InterpretFPRes(
    Interpretation::Code code,
    Interpretation::Frame frame,
    Interpretation::Value::Primitive fr0,
    Interpretation::Value::Primitive fr1,
    uintptr_t frameBot
)
{
    return Interpretation::Interpret<Cbc::FReg>(
        code, frame, U32(0), U32(0), fr0, fr1, Cbc::FReg::FR0, U64(0), frameBot
    );
}

MockInterfaceDispatch& InterfaceDispatchMock()
{
    static MockInterfaceDispatch mock;
    return mock;
}

void InitializeMockInterpreter()
{
    using namespace Interpretation;
    Engine::InitEnvOptions();
    InterfaceDispatchMock() = {};
    auto i2call = reinterpret_cast<Interpretation::I2Call>(&Interpretation::InterpreterI2CallTest);
}

namespace RTSupport {

using Reference = Interpretation::Value::Reference;

// A TypeInfo embedded in bytecode is resolved at rewrite time and travels to run time as a raw
// pointer (NEWOBJ/NEWARR/NEWBOX2/IOF embed TypeInfo::Raw() as an immediate; CALL_INTERF carries it
// in the instruction word). MockNewObj/MockNewArr write that same pointer into the object header,
// and Execution::GetTypeInfo reads it back, so the mock must hand out one immortal TestTypeInfo
// per interned term: the registry is keyed by the interned term identity (GlobalTerm::operator==
// compares the interned TermData pointers) and entries are never destroyed.
//
// Shape: non-primitive terms get a generic object-like TI (header 8 + one payload slot, matching
// the mocked MetaInfo constants and what MockNewObj allocates). Primitive terms mirror
// Engine::FieldLayoutManager::GetFlatSize so that TI-derived addressing (LeaIndex stride,
// copy sizes) stays consistent with the kind-derived strides of ld.arr/st.arr and the
// load/store access widths.
static TestTypeInfo& SyntheticTypeInfo(Engine::GlobalTerm term)
{
    using TK = Engine::TermKind;
    size_t size = 2 * MetaInfo::ObjectHeaderSize();
    uint8_t alignment = sizeof(void*);
    switch (term.GetKind()) {
        case TK::VOID:
        case TK::UNIT:          size = 0; alignment = 1; break;
        case TK::I8:
        case TK::U8:
        case TK::BOOLEAN:       size = 1; alignment = 1; break;
        case TK::I16:
        case TK::U16:
        case TK::F16:           size = 2; alignment = 2; break;
        case TK::I32:
        case TK::U32:
        case TK::F32:
        case TK::UCHAR32:       size = 4; alignment = 4; break;
        case TK::I64:
        case TK::U64:
        case TK::F64:
        case TK::IADDR:
        case TK::UADDR:
        case TK::BSTRING:
        case TK::C_POINTER:
        case TK::NULLABLE:
        case TK::FUNCTIONAL:
        case TK::NON_NULLABLE:  size = sizeof(void*); alignment = sizeof(void*); break;
        default:                break;
    }
    static std::unordered_map<uintptr_t, TestTypeInfo> registry;
    return registry.try_emplace(reinterpret_cast<uintptr_t>(term.data), size, alignment).first->second;
}

std::optional<TypeInfo> CreateTypeInfo(Engine::Session& session, TypeInfoManager& manager, Engine::GlobalTerm term)
{
    return TypeInfo(&SyntheticTypeInfo(term));
}

Engine::GlobalTerm ReconstructTerm(Engine::Session& session, TypeInfoManager& manager, TypeInfo ti)
{
    FATAL("Should not be called");
}

void Execution::WriteGeneric(Reference base, uintptr_t field, Reference object, size_t size, ThreadHandle th)
{
    FATAL("Should not be called");
}

void Execution::ReadStaticStruct(uintptr_t dst, uintptr_t src, TypeInfo ti, ThreadHandle th)
{
    FATAL("Should not be called");
}

void Execution::WriteStaticStruct(uintptr_t dst, uintptr_t src, TypeInfo ti, ThreadHandle th)
{
    FATAL("Should not be called");
}

Reference Execution::ReadObjectInstance(Reference base, uintptr_t field, ThreadHandle th)
{
    return Reference { .value = *reinterpret_cast<uintptr_t*>(field) };
}

void Execution::WriteObjectInstance(Reference base, uintptr_t field, Reference object, ThreadHandle th)
{
    *reinterpret_cast<uintptr_t*>(field) = object.value;
}

void Execution::WriteStructField(uintptr_t src, Reference base, uintptr_t field, TypeInfo ti, ThreadHandle th)
{
    FATAL("Should not be called");
}

void Execution::ReadStructField(uintptr_t dst, Reference base, uintptr_t field, TypeInfo ti, ThreadHandle th)
{
    FATAL("Should not be called");
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

void Execution::InitializeClosure(Reference closure, bool instantiatedSret) { FATAL("Should not reach here."); }

Interpretation::Thunk Execution::GetClosureThunk(Reference base, bool isInstantiated)
{
    FATAL("Should not reach here.");
}

Interpretation::Thunk Execution::GetVirtualThunk(Reference base, int extDefNum, int methodNum)
{
    FATAL("Should not reach here. I2C virtual call");
}

TypeInfo Execution::GetMethodOuterTi(TypeInfo where, TypeInfo interf, int methodNum)
{
    auto& mock = InterfaceDispatchMock();
    ASSERT(mock.enabled);
    mock.outerTiCalls.PushBack({ where, interf, methodNum });
    return mock.outerTi;
}

Interpretation::Thunk Execution::GetInterfaceThunk(TypeInfo where, TypeInfo ti, int methodNum)
{
    auto& mock = InterfaceDispatchMock();
    ASSERT(mock.enabled);
    mock.dispatchCalls.PushBack({ where, ti, methodNum });
    return mock.thunk;
}

void* Execution::GcPoint() { FATAL("Should not reach here"); }

bool Execution::IsPendingSafePoint() { return false; }

bool Execution::IsInstanceOf(Reference base, TypeInfo ti) { FATAL("Should not reach here"); }

TypeInfo Execution::LoadTypeInfo(Engine::GlobalTerm term, Interpretation::Ectype* ectype, void* stackSlots)
{
    FATAL("Should not reach here");
}

bool Execution::IsReference(TypeInfo ti) { return true; }

StructLocationKind Execution::GetStructLocationKind(Reference base, uintptr_t derived)
{
    if (base.value == 0) {
        return LOCAL;
    }
    return HEAP;
}

Reference Execution::GetGlobalBasePtr() { FATAL("Should not reach here"); }

Reference Execution::GetLocalBasePtr() { FATAL("Should not reach here"); }

uintptr_t Execution::DerivedPtrOfGlobal(uintptr_t derived) { FATAL("Should not reach here"); }

uintptr_t Execution::UnwrapDerivedPtr(uint64_t derived, bool checked) { FATAL("Should not reach here"); }

Reference Execution::GetPendingException()
{
    FATAL("Should not reach here");
    return Reference { .value = 0 };
}

Reference Execution::GetAndClearPendingException()
{
    FATAL("Should not reach here");
    return Reference { .value = 0 };
}

Reference Execution::AtomicReadRef(Reference object, uintptr_t field)
{
    FATAL("Should not reach here");
    return Reference { .value = 0 };
}

void Execution::AtomicWriteRef(Reference ref, Reference obj, uintptr_t field) { FATAL("Should not reach here"); }

Reference Execution::AtomicSwapRef(Reference ref, Reference obj, uintptr_t field)
{
    FATAL("Should not reach here");
    return Reference { .value = 0 };
}

bool Execution::AtomicCompareAndSwapRef(Reference oldRef, Reference newRef, Reference obj, uintptr_t field)
{
    FATAL("Should not reach here");
}

static void C2ICall() { FATAL("Should not reach here. Mock c2i"); }

void* Adapters::GetDynCallTrampoline(int idx, bool sret, bool isStatic) { FATAL("Should not reach here"); }

void* Adapters::GenericC2ICallInstance() { return reinterpret_cast<void*>(&C2ICall); }

void* Adapters::IregOnlyC2ICallInstance() { return reinterpret_cast<void*>(&C2ICall); }

void* Adapters::C2ICall(Interpretation::AbiInfo info) { return reinterpret_cast<void*>(&C2ICall); }

void* Adapters::GetDirectCallTrampoline(Interpretation::DynamicFunctionHandle* fuh) { FATAL("Should not reach here."); }

const char* MetaInfo::GetName(TypeInfo ti) { return "<unknown>"; }

uint32_t MetaInfo::GetTypeSize(TypeInfo ti) { return Interpretation::Extract(ti)->size; }

uint32_t MetaInfo::GetAlignedSize(TypeInfo ti)
{
    if (IsReferenceType(ti)) {
        return sizeof(void*);
    }
    auto size      = GetTypeSize(ti);
    auto alignment = GetAlign(ti);
    return alignment != 0 ? MathUtils::AlignUp(size, alignment) : size;
}

uint8_t MetaInfo::GetAlign(TypeInfo ti) { return Interpretation::Extract(ti)->alignment; }

bool MetaInfo::IsReferenceType(TypeInfo ti) { return false; }

TypeInfo MetaInfo::ByteArrayTypeInfo() { return TypeInfo(nullptr); }

TypeInfoUUID MetaInfo::GetUUID(TypeInfo ti)
{
    // Engine::TypeInfoManager FATALs ("Unexpected UUID associated term") when two created
    // TypeInfos share a UUID, so the id must be unique per TypeInfo instance; instance
    // addresses are stable and 8-aligned, for registry-owned and test-fabricated TIs alike.
    return static_cast<TypeInfoUUID>(reinterpret_cast<uintptr_t>(ti.Raw()) >> 3);
}

TypeInfo MetaInfo::GetElementTypeInfo(TypeInfo ti) { return TypeInfo(nullptr); }

TypeInfo Execution::TypeArg(TypeInfo ti, uint32_t idx) { return TypeInfo(nullptr); }

using OffsetVisitor = Utils::Function<void(uint32_t)>;

void TypeInfo::VisitReferenceOffsets(OffsetVisitor const& visitor)
{
    for (auto offset : Interpretation::Extract(*this)->referenceOffsets) {
        visitor(offset);
    }
}

} // namespace RTSupport
