#include "runtimesupport/runtime.h"
#include "RTInterface.h"
#include "RuntimeTypes.h"
#include "asm_trampolines.h"
#include "cjnative.h"
#include "runtimesupport/impl/rt_syms.h"
#include "runtimesupport/impl/typeinfo_ext.h"
#include "utils/assertion.h"

namespace RTSupport {

using Reference = Interpretation::Value::Reference;

Reference Execution::ReadObjectInstance(Reference base, size_t offset, ThreadHandle th)
{
    return Reference { .value = reinterpret_cast<uintptr_t>(g_CJNativeInterfaceInstance.readInstanceField(
                           reinterpret_cast<DYN_ObjRefT>(base.value),
                           reinterpret_cast<DYN_FieldRefT>(base.value + offset)
                       )) };
}

void Execution::WriteObjectInstance(Reference base, size_t offset, Reference object, ThreadHandle th)
{
    g_CJNativeInterfaceInstance.writeInstanceField(
        reinterpret_cast<DYN_ObjRefT>(base.value),
        reinterpret_cast<DYN_FieldRefT>(base.value + offset),
        reinterpret_cast<DYN_ObjRefT>(object.value)
    );
}

Reference Execution::ReadObjectStatic(void* location, ThreadHandle th)
{
    return Reference { .value = reinterpret_cast<uintptr_t>(
                           g_CJNativeInterfaceInstance.readStaticField(reinterpret_cast<DYN_FieldRefT>(location))
                       ) };
}

void Execution::WriteObjectStatic(void* location, Reference object, ThreadHandle th)
{
    g_CJNativeInterfaceInstance.writeStaticField(
        reinterpret_cast<DYN_FieldRefT>(location), reinterpret_cast<DYN_ObjRefT>(object.value)
    );
}

void* Execution::AllocateObjectInstance() { return reinterpret_cast<void*>(&Asm::engine_i2_newobject); }

void* Execution::GcPoint() { return reinterpret_cast<void*>(g_CJNativeInterfaceInstance.safePoint); }

void* Execution::GcPointTrampoline() { return reinterpret_cast<void*>(&Asm::engine_i2_gcpoint); }

bool Execution::IsPendingSafePoint()
{
    return g_CJNativeInterfaceInstance.isPendingSafePoint(g_CJNativeInterfaceInstance.getThreadLocalData());
}

TypeInfo Execution::GetTypeInfo(Reference base)
{
    TypeInfo* header = reinterpret_cast<TypeInfo*>(base.value);
    return *header;
}

void* Execution::GetVirtualTarget(Reference base, int extDefNum, int methodNum)
{
    DYN_TypeInfoT** header = reinterpret_cast<DYN_TypeInfoT**>(base.value);
    auto typeInfo          = *header;
    auto target            = typeInfo->vExtensionDataStart[extDefNum]->funcTable[methodNum];
    return target;
}

void* Execution::GetInterfaceTarget(Reference base, TypeInfo interf, int methodNum)
{
    DYN_TypeInfoT** header = reinterpret_cast<DYN_TypeInfoT**>(base.value);
    auto typeInfo          = *header;
    DYN_FuncPtrT* table    = GetMTable(typeInfo, UnpackTypeInfo(interf));
    return table[methodNum];
}

int Execution::GetFieldOffset(TypeInfo ti, int ordinal, bool isRef)
{
    auto mrtti      = UnpackTypeInfo(ti);
    auto headerOffs = isRef ? 8 : 0;
    ASSERT(ordinal < mrtti->fieldNum);
    return mrtti->fieldOffsets[ordinal] + headerOffs;
}

std::optional<int> MetaInfo::GetTypeSize(std::optional<TypeInfo> t)
{
    if (!t.has_value()) {
        return std::nullopt;
    }

    auto mrtti = UnpackTypeInfo(t.value());
    return mrtti->instanceSize;
}

} // namespace RTSupport
