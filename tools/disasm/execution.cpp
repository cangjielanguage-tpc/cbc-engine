#include <cstring>

#include "engine/typeinfo_manager.h"
#include "interpreter/function_handle.h"
#include "runtimesupport/adapters.h"
#include "runtimesupport/runtime.h"
#include "utils/assertion.h"

static constexpr int HEAP_SIZE = 16384;

namespace RTSupport {

using Reference = Interpretation::Value::Reference;

std::optional<TypeInfo> CreateTypeInfo(
    Engine::Session& session, Engine::TypeInfoManager& manager, Symlevel::GlobalTerm term
)
{
    FATAL("Should not be called");
}

Reference Execution::ReadObjectInstance(Reference base, size_t offset, ThreadHandle th)
{
    FATAL("Should not be called :)");
}

void Execution::WriteObjectInstance(Reference base, size_t offset, Reference object, ThreadHandle th)
{
    FATAL("Should not be called");
}

Reference Execution::ReadObjectStatic(void* location, ThreadHandle th) { FATAL("Should not be called"); }

void Execution::WriteObjectStatic(void* location, Reference object, ThreadHandle th) { FATAL("Should not be called"); }

int Execution::GetFieldOffset(TypeInfo ti, int ordinal, bool isRef)
{
    FATAL("Should not reach here. Get field offset");
}

TypeInfo Execution::GetTypeInfo(Reference base) { FATAL("Should not be called"); }

MethodTable Execution::GetMethodTable(Reference base, int extDefNum, int methodNum)
{
    FATAL("Should not reach here. I2C virtual call");
}

void* Execution::AllocateObjectInstance() { FATAL("Should not reach here"); }

void* Execution::GcPointTrampoline() { FATAL("Should not reach here"); }

void* Execution::GcPoint() { FATAL("Should not reach here"); }

bool Execution::IsPendingSafePoint() { return false; }

void* Adapters::GenericI2CCallInstance() { FATAL("Should not reach here. Mock i2c"); }

void* Adapters::I2ICallInstance() { FATAL("Should not reach here"); }

static void C2ICall() { FATAL("Should not reach here. Mock c2i"); }

void* Adapters::IregOnlyC2ICallInstance() { FATAL("Should not reach here."); }

void* Adapters::GetDirectCallTrampoline(Interpretation::DynamicFunctionHandle* fuh) { FATAL("Should not reach here."); }

} // namespace RTSupport
