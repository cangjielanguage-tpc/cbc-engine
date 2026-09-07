#pragma once

/// This file defines Runtime specific interface for communication between
/// interpreter and the runtime.

#include "engine/terms.h"
#include "interpreter/ectype.h"
#include "interpreter/int_thunk.h"
#include <cstdint>
#include <functional>

namespace RTSupport {

// TypeInfo flags
static constexpr uint64_t GCTIB_SIGN_BIT = (1lu << 63);

#if defined(__x86_64__) || defined(_M_X64)
static constexpr uintptr_t DERIVED_PTR_GLOBAL_FLAG = 0x1;
#elif defined(__aarch64__) || defined(_M_ARM64)
static constexpr uintptr_t DERIVED_PTR_GLOBAL_FLAG = 1ULL << 63;
#endif

using TypeInfoUUID = uint32_t;

using OffsetVisitor = std::function<void(uint32_t)>;

enum StructLocationKind {
    LOCAL,
    GLOBAL,
    HEAP,
};

class ThreadHandle {
public:
    explicit ThreadHandle(void* _value) : value(_value) {}

    inline void* Raw() const { return value; }

private:
    void* value;
};

class TypeInfo {
public:
    explicit TypeInfo(uintptr_t _value) : value(reinterpret_cast<void*>(_value)) {}

    explicit TypeInfo(void* value) : value(value) {}

    TypeInfo() : value(nullptr) {}

    void VisitReferenceOffsets(OffsetVisitor const& f);

    inline void* Raw() const { return value; }

    inline uintptr_t UInt() const { return reinterpret_cast<uintptr_t>(value); }

private:
    void* value;
};

// should have the same layout as StdGCTib in cangjie_runtime/runtime/src/ObjectModel/MClass.h
struct StdGCTib {
    uint32_t nBitmapWords;
    uint8_t bitmapWords[];
};

// should have the same layout as ShortGCTib in cangjie_runtime/runtime/src/ObjectModel/MClass.h
struct ShortGCTib {
    uintptr_t bitmap;
};

struct Execution {
    using Reference = Interpretation::Value::Reference;

    /// Each element represent an function that accepts (Ectype, ThreadHandle, TypeInfo)
    /// and puts result in IReg(idx) register.
    ///
    /// This specialization is needed to allow Thunk usage.

    // dst = IR1
    static void* AllocateObjectInstance();

    // dst = IR1
    static void* AllocateObjectPinnedInstance();

    // dst = IR_ACC
    static void* AllocateObjectInstanceAcc();

    // dst = IR1
    static void* AllocateArrayInstance();

    static void* LoadGeneric();

    static void* HandleException();

    static void* ThrowImplicitException();

    static void* GcPoint();

    static void* GcPointTrampoline();

    static void* Spawn();
    static void* SpawnFuture();

    static bool IsPendingSafePoint();

    static size_t ArrayLength(Reference array);

    static void WriteGeneric(Reference base, uintptr_t field, Reference object, size_t size, ThreadHandle th);
    static Reference ReadObjectInstance(Reference base, uintptr_t field, ThreadHandle th);
    static void WriteObjectInstance(Reference base, uintptr_t field, Reference object, ThreadHandle th);
    static Reference ReadArrayElem(Reference array, uint64_t index, ThreadHandle th);
    static void WriteArrayElem(Reference array, uint64_t index, Reference object, ThreadHandle th);
    static Reference ReadObjectStatic(void* location, ThreadHandle th);
    static void WriteObjectStatic(void* location, Reference object, ThreadHandle th);

    static void ReadStaticStruct(uintptr_t dst, uintptr_t src, TypeInfo ti, ThreadHandle th);
    static void WriteStaticStruct(uintptr_t dst, uintptr_t src, TypeInfo ti, ThreadHandle th);

    static void WriteStructField(uintptr_t src, Reference base, uintptr_t field, TypeInfo ti, ThreadHandle th);
    static void ReadStructField(uintptr_t dst, Reference base, uintptr_t field, TypeInfo ti, ThreadHandle th);

    static TypeInfo GetTypeInfo(Reference base);

    static Interpretation::Thunk GetClosureThunk(Reference base, bool isInstantiated);

    static Interpretation::Thunk GetVirtualThunk(Reference base, int extDefNum, int methodNum);

    static TypeInfo GetMethodOuterTi(TypeInfo where, TypeInfo interf, int methodNum);
    static Interpretation::Thunk GetInterfaceThunk(TypeInfo where, TypeInfo ti, int methodNum);

    static uint32_t GetFieldOffset(TypeInfo ti, int ordinal, bool adjustByHeader);

    static bool IsInstanceOf(Reference base, TypeInfo ti);

    static TypeInfo LoadTypeInfo(Engine::GlobalTerm term, Interpretation::Ectype* ectype, void* stackSlots);

    static bool IsReference(TypeInfo ti);
    static StructLocationKind GetStructLocationKind(Reference base, uintptr_t derived);
    static Reference GetGlobalBasePtr();
    static Reference GetLocalBasePtr();

    static TypeInfo TypeArg(TypeInfo ti, uint32_t idx);

    static Reference GetPendingException();
    static Reference GetAndClearPendingException();

    static Reference AtomicReadRef(Reference object, uintptr_t field);
    static void AtomicWriteRef(Reference ref, Reference obj, uintptr_t field);
    static Reference AtomicSwapRef(Reference ref, Reference obj, uintptr_t field);
    static bool AtomicCompareAndSwapRef(Reference oldRef, Reference newRef, Reference obj, uintptr_t field);
};

struct MetaInfo {
    static const char* GetName(TypeInfo ti);
    static uint32_t GetTypeSize(TypeInfo ti);
    static uint8_t GetAlign(TypeInfo ti);

    static bool IsReferenceType(TypeInfo ti);

    static uint32_t ObjectHeaderSize() { return sizeof(void*); }

    static uint32_t ArrayBodyOffset() { return sizeof(void*) + sizeof(uint64_t); }

    static TypeInfo ByteArrayTypeInfo();

    static TypeInfoUUID GetUUID(TypeInfo ti);
};

} // namespace RTSupport
