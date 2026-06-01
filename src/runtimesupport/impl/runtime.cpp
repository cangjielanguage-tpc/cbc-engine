#include "runtimesupport/runtime.h"
#include "RTInterface.h"
#include "RuntimeTypes.h"
#include "asm_trampolines.h"
#include "cjnative.h"
#include "runtimesupport/impl/rt_syms.h"
#include "runtimesupport/impl/typeinfo_ext.h"
#include "utils/assertion.h"
#include <cstdint>

namespace RTSupport {

using Reference = Interpretation::Value::Reference;

Reference Execution::ReadObjectInstance(Reference base, size_t offset, ThreadHandle th)
{
    return Reference { .value = reinterpret_cast<uintptr_t>(g_CJNativeInterfaceInstance.readInstanceField(
                           reinterpret_cast<DYN_ObjRef>(base.value), reinterpret_cast<DYN_FieldRef>(base.value + offset)
                       )) };
}

void Execution::WriteObjectInstance(Reference base, size_t offset, Reference object, ThreadHandle th)
{
    g_CJNativeInterfaceInstance.writeInstanceField(
        reinterpret_cast<DYN_ObjRef>(base.value),
        reinterpret_cast<DYN_FieldRef>(base.value + offset),
        reinterpret_cast<DYN_ObjRef>(object.value)
    );
}

Reference Execution::ReadObjectStatic(void* location, ThreadHandle th)
{
    return Reference { .value = reinterpret_cast<uintptr_t>(
                           g_CJNativeInterfaceInstance.readStaticField(reinterpret_cast<DYN_FieldRef>(location))
                       ) };
}

void Execution::WriteObjectStatic(void* location, Reference object, ThreadHandle th)
{
    g_CJNativeInterfaceInstance.writeStaticField(
        reinterpret_cast<DYN_FieldRef>(location), reinterpret_cast<DYN_ObjRef>(object.value)
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
    DYN_TypeInfo** header  = reinterpret_cast<DYN_TypeInfo**>(base.value);
    auto typeInfo          = *header;
    auto target            = typeInfo->vExtensionDataStart[extDefNum]->funcTable[methodNum];
    return target;
}

void* Execution::GetInterfaceTarget(Reference base, TypeInfo interf, int methodNum)
{
    DYN_TypeInfo** header  = reinterpret_cast<DYN_TypeInfo**>(base.value);
    auto typeInfo          = *header;
    DYN_FuncPtr* table     = g_CJNativeInterfaceInstance.getMTable(typeInfo, UnpackTypeInfo(interf));
    return table[methodNum];
}

int Execution::GetFieldOffset(TypeInfo ti, int ordinal, bool adjustByHeader)
{
    auto mrtti      = UnpackTypeInfo(ti);
    auto headerOffs = adjustByHeader ? sizeof(void*) : 0;
    ASSERT(ordinal < mrtti->fieldNum);
    return mrtti->fieldOffsets[ordinal] + headerOffs;
}

bool Execution::IsInstanceOf(Reference base, TypeInfo ti)
{
    return g_CJNativeInterfaceInstance.instanceOf(reinterpret_cast<DYN_ObjRef>(base.value), UnpackTypeInfo(ti));
}

uint32_t MetaInfo::GetTypeSize(TypeInfo ti)
{
    auto mrtti = UnpackTypeInfo(ti);
    return mrtti->instanceSize;
}

uint8_t MetaInfo::GetAlign(TypeInfo ti)
{
    auto mrtti = UnpackTypeInfo(ti);
    return mrtti->align;
}

bool MetaInfo::IsReferenceType(TypeInfo ti)
{
    auto mrtti = UnpackTypeInfo(ti);
    return mrtti->type < 0;
}

void MetaInfo::VisitReferences(TypeInfo ti, std::function<void(uint32_t)> visitor)
{
    auto mrtti = UnpackTypeInfo(ti);

    uintptr_t SHORT_GCTIB_TAG = 1ull << (8 * sizeof(uintptr_t) - 1);
    if ((mrtti->gctib.raw & SHORT_GCTIB_TAG) == 0) {
        FATAL("pointer gctib format is not supported yet");
        return;
    }

    auto bitmap = mrtti->gctib.raw & ~SHORT_GCTIB_TAG;
    uint32_t startOffset = IsReferenceType(ti) ? ObjectHeaderSize() : 0;
    for (uint32_t offset = startOffset; bitmap != 0; offset += sizeof(uintptr_t)) {
        if ((bitmap & 1) != 0) {
            visitor(offset);
        }
        bitmap >>= 1;
    }
}

TypeInfo MetaInfo::ByteArrayTypeInfo()
{
    auto typeName = "RawArray<UInt8>";
    auto ti       = g_CJNativeInterfaceInstance.typeInfo(typeName);
    ASSERT(ti != nullptr);

    return TypeInfo(ti);
}

} // namespace RTSupport
