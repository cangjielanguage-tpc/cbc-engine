#include "runtimesupport/runtime.h"
#include "RTInterface.h"
#include "RuntimeTypes.h"
#include "asm_trampolines.h"
#include "cjnative.h"
#include "engine/engine.h"
#include "engine/terms.h"
#include "engine/typeinfo_manager.h"
#include "interpreter/ectype.h"
#include "interpreter/function_handle.h"
#include "interpreter/interpretation_loop.h"
#include "runtimesupport/adapters.h"
#include "runtimesupport/impl/rt_syms.h"
#include "runtimesupport/impl/typeinfo_ext.h"
#include "utils/assertion.h"
#include <cstddef>
#include <cstdint>

namespace RTSupport {

using Reference = Interpretation::Value::Reference;

void Execution::WriteGeneric(Reference base, uintptr_t field, Reference object, size_t size, ThreadHandle th)
{
    RTSupport::WriteGeneric(base.value, field, object.value, size);
}

Reference Execution::ReadObjectInstance(Reference base, uintptr_t field, ThreadHandle th)
{
    return Reference { .value = reinterpret_cast<uintptr_t>(g_CJNativeInterfaceInstance.readInstanceField(
                           reinterpret_cast<DYN_ObjRef>(base.value), reinterpret_cast<DYN_FieldRef>(field)
                       )) };
}

void Execution::WriteObjectInstance(Reference base, uintptr_t field, Reference object, ThreadHandle th)
{
    g_CJNativeInterfaceInstance.writeInstanceField(
        reinterpret_cast<DYN_ObjRef>(base.value),
        reinterpret_cast<DYN_FieldRef>(field),
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

void Execution::WriteStructField(uintptr_t src, Reference base, uintptr_t field, TypeInfo ti, ThreadHandle th)
{
    auto type = UnpackTypeInfo(ti);
    auto size = type->instanceSize;
    RTSupport::WriteStructField(base.value, field, size, src, size, type->gctib);
}

void Execution::ReadStructField(uintptr_t dst, Reference base, uintptr_t field, TypeInfo ti, ThreadHandle th)
{
    auto type = UnpackTypeInfo(ti);
    RTSupport::ReadStructField(dst, base.value, field, type->instanceSize, type->gctib);
}

void* Execution::AllocateObjectInstance() { return reinterpret_cast<void*>(&Asm::engine_i2_newobject); }

void* Execution::AllocateObjectInstanceAcc() { return reinterpret_cast<void*>(&Asm::engine_i2_newobject_acc); }

void* Execution::AllocateArrayInstance() { return reinterpret_cast<void*>(&Asm::engine_i2_newarray); }

void* Execution::GcPoint() { return reinterpret_cast<void*>(g_CJNativeInterfaceInstance.safePoint); }

void* Execution::GcPointTrampoline() { return reinterpret_cast<void*>(&Asm::engine_i2_gcpoint); }

void* Execution::LoadGeneric() { return reinterpret_cast<void*>(&Asm::engine_i2_load_generic); }

void* Execution::Spawn() { return reinterpret_cast<void*>(&Asm::engine_i2_spawn); }

bool Execution::IsPendingSafePoint()
{
    return g_CJNativeInterfaceInstance.isPendingSafePoint(g_CJNativeInterfaceInstance.getThreadLocalData()) != 0;
}

TypeInfo Execution::GetTypeInfo(Reference base)
{
    TypeInfo* header = reinterpret_cast<TypeInfo*>(base.value);
    return *header;
}

static char* GetDynCallTrampolinesStart() { return reinterpret_cast<char*>(&Asm::engine_trampolines_dyn_start); }

static size_t GetDynCallTrampolinesLength()
{
    auto begin = GetDynCallTrampolinesStart();
    auto end   = reinterpret_cast<char*>(&Asm::engine_trampolines_dyn_end);
    return end - begin;
}

static bool IsDynCallTrampoline(void* function)
{
    auto trampolinesStart = reinterpret_cast<size_t>(GetDynCallTrampolinesStart());
    auto funcPos          = reinterpret_cast<size_t>(function) - trampolinesStart;
    return funcPos <= GetDynCallTrampolinesLength();
}

static size_t DynCallTrampolineIdx(void* trampoline)
{
    ASSERT(IsDynCallTrampoline(trampoline));
    auto trampolinesStart = reinterpret_cast<size_t>(GetDynCallTrampolinesStart());
    auto funcIdx          = (reinterpret_cast<size_t>(trampoline) - trampolinesStart) / DYN_CALL_TRAMPOLINE_SIZE;
    return funcIdx;
}

static Interpretation::FunctionHandle* GetDynamicCall(void* fn, CbcTypeInfo* cti)
{
    return cti->dataMT[DynCallTrampolineIdx(fn)];
}

static Interpretation::Thunk GetDynCallThunk(void* fn, TypeInfo ti)
{
    if (IsDynCallTrampoline(fn)) {
        // Fast path: it is trampoline, meaning i2i call. We just get fuh and run i2i call as usual.
        auto fuh = GetDynamicCall(fn, reinterpret_cast<CbcTypeInfo*>(ti.Raw()));
        return { Adapters::I2ICallInstance(), reinterpret_cast<void*>(fuh) };
    }

    return { Adapters::GenericI2CCallInstance(), fn };
}

Interpretation::Thunk Execution::GetVirtualThunk(Reference base, int extDefNum, int methodNum)
{
    DYN_TypeInfo** header = reinterpret_cast<DYN_TypeInfo**>(base.value);
    auto dynTypeInfo      = *header;
    auto target           = dynTypeInfo->vExtensionDataStart[extDefNum]->funcTable[methodNum];
    auto typeInfo         = TypeInfo(dynTypeInfo);
    return GetDynCallThunk(target, typeInfo);
}

Interpretation::Thunk Execution::GetInterfaceThunk(Reference base, TypeInfo interf, int methodNum)
{
    DYN_TypeInfo** header = reinterpret_cast<DYN_TypeInfo**>(base.value);
    auto dynTypeInfo      = *header;
    DYN_FuncPtr* table    = g_CJNativeInterfaceInstance.getMTable(dynTypeInfo, UnpackTypeInfo(interf));
    auto target           = table[methodNum];

    auto typeInfo = TypeInfo(dynTypeInfo);
    return GetDynCallThunk(target, typeInfo);
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
    return g_CJNativeInterfaceInstance.instanceOf(reinterpret_cast<DYN_ObjRef>(base.value), UnpackTypeInfo(ti)) != 0;
}

bool Execution::IsReference(TypeInfo ti)
{
    // Reference type are encoded with negative int8_t values.
    // @see typeinfo_factory.cpp
    return UnpackTypeInfo(ti)->type < 0;
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

Reference Execution::GetLocalBasePtr() { return Reference { .value = 0 }; }

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

    auto bitmap          = mrtti->gctib.raw & ~GCTIB_SIGN_BIT;
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

TypeInfo Execution::TypeArg(TypeInfo ti, uint32_t idx)
{
    auto typeInfo = UnpackTypeInfo(ti);
    return TypeInfo(typeInfo->typeArgs[idx]);
}

TypeInfoUUID MetaInfo::GetUUID(TypeInfo ti) { return g_CJNativeInterfaceInstance.getTypeInfoUUID(UnpackTypeInfo(ti)); }

TypeInfo Execution::LoadTypeInfo(Engine::GlobalTerm term, Interpretation::Ectype* ectype, void* stackSlots)
{
    // FIXME: optimize!
    auto length = term.GetLength();
    if (length > 6) {
        FATAL("not supported yet");
    }
    Engine::Session session(Engine::GetEngineInstance());
    auto& tiManager = Engine::TypeInfoManager::Of(session);
    std::vector<Engine::Term> terms;
    for (int i = 0; i < length; i++) {
        auto ti = reinterpret_cast<DYN_TypeInfo*>(ectype->iregs[1 + i].primitive.u64);
        terms.push_back(tiManager.AcquireTerm(session, TypeInfo(ti)));
    }

    Engine::ArraySubstitution sub(session, terms);
    auto type = sub.Substitute(term);

    // resolution error should be handled in rewriter.
    auto res = tiManager.AcquireTypeInfo(session, type);
    return res.value();
}

} // namespace RTSupport
