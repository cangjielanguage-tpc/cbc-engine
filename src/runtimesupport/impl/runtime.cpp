#include "runtimesupport/runtime.h"
#include "RTInterface.h"
#include "RuntimeTypes.h"
#include "asm_trampolines.h"
#include "cjnative.h"
#include "interpreter/implicit_exceptions.h"
#include "runtimesupport/impl/rt_syms.h"
#include "runtimesupport/impl/typeinfo_ext.h"
#include "utils/assertion.h"
#include <cstdint>

namespace RTSupport {

using Reference = Interpretation::Value::Reference;

/// Executes Cangjie native @C function if it should be called directly from interpreter.
/// Performs N2C transition in terms of CJNative runtime.
///
/// Defined in N2C.S
extern "C" void* engine_execute_cangjie_cfunc(...);

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

Reference Execution::ReadArrayElem(Reference array, uint64_t index, ThreadHandle th)
{
    return Reference { .value = reinterpret_cast<uintptr_t>(g_CJNativeInterfaceInstance.getArrayRefElement(
                           reinterpret_cast<DYN_ObjRef>(array.value), index
                       )) };
}

void Execution::WriteArrayElem(Reference array, uint64_t index, Reference object, ThreadHandle th)
{
    g_CJNativeInterfaceInstance.setArrayRefElement(
        reinterpret_cast<DYN_ObjRef>(array.value), index, reinterpret_cast<DYN_ObjRef>(object.value)
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

void* Execution::AllocateArrayInstance() { return reinterpret_cast<void*>(&Asm::engine_i2_newarray); }

void* Execution::HandleException() { return reinterpret_cast<void*>(&Asm::engine_handle_exception); }

void* Execution::GcPoint() { return reinterpret_cast<void*>(g_CJNativeInterfaceInstance.safePoint); }

void* Execution::GcPointTrampoline() { return reinterpret_cast<void*>(&Asm::engine_i2_gcpoint); }

void* Execution::Spawn() { return reinterpret_cast<void*>(&Asm::engine_i2_spawn); }

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

uint32_t Execution::GetFieldOffset(TypeInfo ti, int ordinal, bool adjustByHeader)
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

bool Execution::IsGlobalStruct(Reference base, uintptr_t derived)
{
#if defined(__x86_64__) || defined(_M_X64)
    return (base.value & DERIVED_PTR_GLOBAL_FLAG) != 0;
#elif defined(__aarch64__) || defined(_M_ARM64)
    return (derived & DERIVED_PTR_GLOBAL_FLAG) != 0;
#endif
}

Reference Execution::GetGlobalBasePtr()
{
#if defined(__x86_64__) || defined(_M_X64)
    return Reference { .value = DERIVED_PTR_GLOBAL_FLAG };
#elif defined(__aarch64__) || defined(_M_ARM64)
    return Reference { .value = 0 };
#endif
}

Reference Execution::GetLocalBasePtr()
{
    return Reference { .value = 0 };
}

void* Execution::ExecuteCangjieCFunc(void* func, uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
    DYN_ThreadLocalData tld = g_CJNativeInterfaceInstance.getThreadLocalData();
    return engine_execute_cangjie_cfunc(arg1, arg2, arg3, func, tld);
}

void* Execution::GetImplicitExceptionsThrower() { return GetSymbolAddr("libhelper.so", "throwImplicitException"); }

Reference Execution::GetPendingException()
{
    return Reference { .value = reinterpret_cast<uintptr_t>(g_CJNativeInterfaceInstance.getPendingException()) };
}

Reference Execution::GetAndClearPendingException()
{
    return Reference { .value =
                           reinterpret_cast<uintptr_t>(g_CJNativeInterfaceInstance.getAndClearPendingException()) };
}

const char* MetaInfo::GetName(TypeInfo ti)
{
    auto mrtti = UnpackTypeInfo(ti);
    return mrtti->typeInfoName;
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

    if ((mrtti->gctib.raw & GCTIB_SIGN_BIT) == 0) {
        FATAL("pointer gctib format is not supported yet");
        return;
    }

    auto bitmap = mrtti->gctib.raw & ~GCTIB_SIGN_BIT;
    uint32_t startOffset = IsReferenceType(ti) ? ObjectHeaderSize() : 0;
    for (uint32_t offset = startOffset; bitmap != 0; offset += sizeof(uintptr_t), bitmap >>= 1) {
        if ((bitmap & 1) != 0) {
            visitor(offset);
        }
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
