#include <cstdint>
#include <cstring>
#include <functional>

#include "engine/typeinfo_manager.h"
#include "interpreter/adapters.h"
#include "interpreter/function_handle.h"
#include "interpreter/interpretation_loop.h"
#include "runtimesupport/adapters.h"
#include "runtimesupport/runtime.h"
#include "utils/assertion.h"

static constexpr int HEAP_SIZE = 16384;

namespace RTSupport {

using Reference = Interpretation::Value::Reference;

std::optional<TypeInfo> CreateTypeInfo(
    Engine::Session& session, Engine::TypeInfoManager& manager, Engine::GlobalTerm term
)
{
    FATAL("Should not be called");
}

Engine::GlobalTerm ReconstructTerm(Engine::Session& session, Engine::TypeInfoManager& manager, TypeInfo ti)
{
    FATAL("Should not be called");
}

void Execution::WriteGeneric(Reference base, uintptr_t field, Reference object, size_t size, ThreadHandle th)
{
    FATAL("Should not be called");
}

Reference Execution::ReadObjectInstance(Reference base, uintptr_t field, ThreadHandle th)
{
    FATAL("Should not be called :)");
}

void Execution::WriteObjectInstance(Reference base, uintptr_t field, Reference object, ThreadHandle th)
{
    FATAL("Should not be called");
}

Reference Execution::ReadArrayElem(Reference array, uint64_t index, ThreadHandle th) { FATAL("Should not be called"); }

void Execution::WriteArrayElem(Reference array, uint64_t index, Reference object, ThreadHandle th)
{
    FATAL("Should not be called");
}

Reference Execution::ReadObjectStatic(void* location, ThreadHandle th) { FATAL("Should not be called"); }

void Execution::WriteObjectStatic(void* location, Reference object, ThreadHandle th) { FATAL("Should not be called"); }

void Execution::WriteStructField(uintptr_t src, Reference base, uintptr_t field, TypeInfo ti, ThreadHandle th)
{
    FATAL("Should not be called");
}

void Execution::ReadStructField(uintptr_t dst, Reference base, uintptr_t field, TypeInfo ti, ThreadHandle th)
{
    FATAL("Should not be called");
}

uint32_t Execution::GetFieldOffset(TypeInfo ti, int ordinal, bool isRef) { FATAL("Should not reach here"); }

TypeInfo Execution::GetTypeInfo(Reference base) { FATAL("Should not be called"); }

Interpretation::Thunk Execution::GetVirtualThunk(Reference base, int extDefNum, int methodNum, uint8_t adapter)
{
    FATAL("Should not reach here.");
}

Interpretation::Thunk Execution::GetInterfaceThunk(Reference base, TypeInfo ti, int methodNum, uint8_t adapter)
{
    FATAL("Should not reach here.");
}

void* Execution::AllocateObjectInstance() { FATAL("Should not reach here"); }

void* Execution::AllocateObjectInstanceAcc() { FATAL("Should not reach here"); }

void* Execution::AllocateArrayInstance() { FATAL("Should not reach here"); }

void* Execution::LoadGeneric() { FATAL("Should not reach here"); }

void* Execution::HandleException()
{
    FATAL("Should not reach here");
    return nullptr;
}

void* Execution::ThrowImplicitException()
{
    FATAL("Should not reach here");
    return nullptr;
}

void* Execution::GcPointTrampoline() { FATAL("Should not reach here"); }

void* Execution::Spawn() { FATAL("Should not reach here"); }

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
    FATAL("Should not reach here");
}

Reference Execution::GetGlobalBasePtr() { FATAL("Should not reach here"); }

Reference Execution::GetLocalBasePtr() { FATAL("Should not reach here"); }

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

void* Adapters::GenericI2CCallInstance() { FATAL("Should not reach here"); }

void* Adapters::GetDynCallTrampoline(int idx) { FATAL("Should not reach here"); }

void* Adapters::I2ICallInstance() { FATAL("Should not reach here"); }

static void C2ICall() { FATAL("Should not reach here"); }

void* Adapters::GenericC2ICallInstance() { FATAL("Should not reach here"); }

void* Adapters::IregOnlyC2ICallInstance() { FATAL("Should not reach here."); }

void* Adapters::GetDirectCallTrampoline(Interpretation::DynamicFunctionHandle* fuh) { FATAL("Should not reach here."); }

const char* MetaInfo::GetName(TypeInfo ti) { return nullptr; }

void* Adapters::C2ICall(uint32_t intArgCount, uint32_t floatArgCount) { FATAL("should not reach here"); }

uint32_t MetaInfo::GetTypeSize(TypeInfo ti) { return 0; }

uint8_t MetaInfo::GetAlign(TypeInfo ti) { return alignof(max_align_t); }

bool MetaInfo::IsReferenceType(TypeInfo ti) { return false; }

TypeInfo MetaInfo::ByteArrayTypeInfo() { return TypeInfo(nullptr); }

TypeInfoUUID MetaInfo::GetUUID(TypeInfo ti) { return 0; }

TypeInfo Execution::TypeArg(TypeInfo ti, uint32_t idx) { return TypeInfo(nullptr); }

using OffsetVisitor = std::function<void(uint32_t)>;

void TypeInfo::VisitReferenceOffsets(OffsetVisitor const&) { FATAL("Should not reach here"); }

} // namespace RTSupport
