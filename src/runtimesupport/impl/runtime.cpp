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
#include "interpreter/implicit_exceptions.h"
#include "interpreter/interpretation_loop.h"
#include "runtimesupport/adapters.h"
#include "runtimesupport/impl/rt_syms.h"
#include "runtimesupport/impl/typeinfo_ext.h"
#include "utils/assertion.h"
#include <cstddef>
#include <cstdint>

namespace RTSupport {
using Reference = Interpretation::Value::Reference;

constexpr uint32_t REF_FIELD_SIZE = sizeof(void*);

static bool IsInlineGCTib(DYN_GCTib tib) { return static_cast<bool>(tib.raw & GCTIB_SIGN_BIT); }

void Execution::WriteGeneric(Reference base, uintptr_t field, Reference object, size_t size, ThreadHandle th)
{
    RTSupport::WriteGeneric(
        reinterpret_cast<DYN_ObjRef>(base.value), field, reinterpret_cast<DYN_ObjRef>(object.value), size
    );
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
    RTSupport::WriteStructField(reinterpret_cast<DYN_ObjRef>(base.value), field, src, size, type->gctib);
}

void Execution::ReadStructField(uintptr_t dst, Reference base, uintptr_t field, TypeInfo ti, ThreadHandle th)
{
    auto type = UnpackTypeInfo(ti);
    RTSupport::ReadStructField(dst, reinterpret_cast<DYN_ObjRef>(base.value), field, type->instanceSize, type->gctib);
}

void* Execution::AllocateObjectInstance() { return reinterpret_cast<void*>(&Asm::engine_i2_newobject); }

void* Execution::AllocateObjectInstanceAcc() { return reinterpret_cast<void*>(&Asm::engine_i2_newobject_acc); }

void* Execution::AllocateArrayInstance() { return reinterpret_cast<void*>(&Asm::engine_i2_newarray); }

void* Execution::HandleException() { return reinterpret_cast<void*>(&Asm::engine_handle_exception); }

void* Execution::ThrowImplicitException() { return reinterpret_cast<void*>(&Asm::engine_throw_implicit_exception); }

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

StructLocationKind Execution::GetStructLocationKind(Reference base, uintptr_t derived)
{
    if (base.value == 0) {
        return LOCAL;
    }

#if defined(__x86_64__) || defined(_M_X64)
    if ((base.value & DERIVED_PTR_GLOBAL_FLAG) != 0) {
        return StructLocationKind::GLOBAL;
    }
#elif defined(__aarch64__) || defined(_M_ARM64)
    if ((derived & DERIVED_PTR_GLOBAL_FLAG) != 0) {
        return StructLocationKind::GLOBAL;
    }
#endif

    return HEAP;
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

Reference Execution::GetPendingException()
{
    return Reference { .value = reinterpret_cast<uintptr_t>(g_CJNativeInterfaceInstance.getPendingException()) };
}

Reference Execution::GetAndClearPendingException()
{
    return Reference { .value =
                           reinterpret_cast<uintptr_t>(g_CJNativeInterfaceInstance.getAndClearPendingException()) };
}

extern "C" Reference engine_get_and_clear_pending_exception() { return Execution::GetAndClearPendingException(); }

Reference Execution::AtomicReadRef(Reference object, uintptr_t field)
{
    return Reference { .value = reinterpret_cast<uintptr_t>(
        g_CJNativeInterfaceInstance.atomicReadRef(reinterpret_cast<DYN_ObjRef>(object.value), reinterpret_cast<DYN_FieldRef>(field))) };
}

void Execution::AtomicWriteRef(Reference ref, Reference obj, uintptr_t field)
{
    g_CJNativeInterfaceInstance.atomicWriteRef(reinterpret_cast<DYN_ObjRef>(ref.value), reinterpret_cast<DYN_ObjRef>(obj.value),
        reinterpret_cast<DYN_FieldRef>(field));
}

Reference Execution::AtomicSwapRef(Reference ref, Reference obj, uintptr_t field)
{
    return Reference { .value = reinterpret_cast<uintptr_t>(
        g_CJNativeInterfaceInstance.atomicSwapRef(reinterpret_cast<DYN_ObjRef>(ref.value), reinterpret_cast<DYN_ObjRef>(obj.value),
            reinterpret_cast<DYN_FieldRef>(field))) };
}

bool Execution::AtomicCompareAndSwapRef(Reference oldRef, Reference newRef, Reference obj, uintptr_t field)
{
    return g_CJNativeInterfaceInstance.atomicCompareAndSwapRef(reinterpret_cast<DYN_ObjRef>(oldRef.value), reinterpret_cast<DYN_ObjRef>(newRef.value),
        reinterpret_cast<DYN_ObjRef>(obj.value), reinterpret_cast<DYN_FieldRef>(field));
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

static void VisitInlineGCTib(ShortGCTib tib, OffsetVisitor const& visitor)
{
    auto info =
        tib.bitmap &
        (~GCTIB_SIGN_BIT); // sign bit signalizes if its a inline or a heap version. it doesn't contain information.
    auto offset = 0;
    while (info != 0) {
        if (info & 1) {
            // indexed field contains reference. visit!
            visitor(offset);
        }
        info   >>= 1;
        offset  += REF_FIELD_SIZE;
    }
}

static void VisitBitmap(uint8_t bitmap, OffsetVisitor const& visitor, size_t offset)
{
    while (bitmap != 0) {
        if (bitmap & 1) {
            visitor(offset);
        }
        bitmap >>= 1;
        offset  += REF_FIELD_SIZE;
    }
}

static void VisitHeapedGCTib(StdGCTib const& gctib, OffsetVisitor const& visitor)
{
    auto offset = 0;
    for (uint32_t i; i < gctib.nBitmapWords; i++) {
        auto bitmap = gctib.bitmapWords[i];
        VisitBitmap(bitmap, visitor, offset);
        offset += REF_FIELD_SIZE * 8;
    }
}

void TypeInfo::VisitReferenceOffsets(OffsetVisitor const& f)
{
    auto mrtti = UnpackTypeInfo(*this);
    auto gctib = mrtti->gctib;
    if (IsInlineGCTib(gctib)) {
        auto wrap = ShortGCTib { gctib.raw };
        VisitInlineGCTib(wrap, f);
    } else {
        auto stdgctib = reinterpret_cast<StdGCTib*>(gctib.ptr);
        VisitHeapedGCTib(*stdgctib, f);
    }
}

} // namespace RTSupport
