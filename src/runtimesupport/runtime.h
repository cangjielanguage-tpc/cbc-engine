#pragma once

/// This file defines Runtime specific interface for communication between
/// interpreter and the runtime.

#include "interpreter/ectype.h"
#include "interpreter/implicit_exceptions.h"
#include <cstdint>
#include <functional>
#include <optional>

namespace RTSupport {

// TypeInfo flags
static constexpr uint8_t HAS_REF_FIELD    = 0b00000001;
static constexpr uint8_t HAS_FINALIZER    = 0b00000010;
static constexpr uint8_t FUTURE_CLASS     = 0b00000100;
static constexpr uint8_t MUTEX_CLASS      = 0b00001000;
static constexpr uint8_t MONITOR_CLASS    = 0b00010000;
static constexpr uint8_t WAIT_QUEUE_CLASS = 0b00100000;
static constexpr uint8_t HAS_REFLECTION   = 0b01000000;
static constexpr uint8_t HAS_EXT_PART     = 0b10000000;

static constexpr uint64_t GCTIB_SIGN_BIT = (1lu << 63);
static constexpr uint32_t GCTIB_MAX_SHORT_OFFSET = sizeof(void*) * 62;

#if defined(__x86_64__) || defined(_M_X64)
    static constexpr uintptr_t DERIVED_PTR_GLOBAL_FLAG = 0x1;
#elif defined(__aarch64__) || defined(_M_ARM64)
    static constexpr uintptr_t DERIVED_PTR_GLOBAL_FLAG = 1ULL << 63;
#endif

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

    inline void* Raw() const { return value; }

private:
    void* value;
};

struct Execution {
    using Reference = Interpretation::Value::Reference;

    /// Each element represent an function that accepts (Ectype, ThreadHandle, TypeInfo)
    /// and puts result in IReg(idx) register.
    ///
    /// This specialization is needed to allow Thunk usage.
    static void* AllocateObjectInstance();

    static void* AllocateArrayInstance();

    static void* HandleException();

    static void* ThrowImplicitException();

    static void* GcPoint();

    static void* GcPointTrampoline();

    static void* Spawn();

    static bool IsPendingSafePoint();

    static size_t ArrayLength(Reference array);

    static Reference ReadObjectInstance(Reference base, size_t offset, ThreadHandle th);
    static void WriteObjectInstance(Reference base, size_t offset, Reference object, ThreadHandle th);
    static Reference ReadArrayElem(Reference array, uint64_t index, ThreadHandle th);
    static void WriteArrayElem(Reference array, uint64_t index, Reference object, ThreadHandle th);
    static Reference ReadObjectStatic(void* location, ThreadHandle th);
    static void WriteObjectStatic(void* location, Reference object, ThreadHandle th);

    static TypeInfo GetTypeInfo(Reference base);

    static void* GetVirtualTarget(Reference base, int extDefNum, int methodNum);
    static void* GetInterfaceTarget(Reference base, TypeInfo ti, int methodNum);

    static uint32_t GetFieldOffset(TypeInfo ti, int ordinal, bool adjustByHeader);

    static bool IsInstanceOf(Reference base, TypeInfo ti);

    static bool IsGlobalStruct(Reference base, uintptr_t derived);
    static Reference GetGlobalBasePtr();
    static Reference GetLocalBasePtr();

    static void RegisterImplicitExceptionsThrower();
    static Reference GetPendingException();
    static Reference GetAndClearPendingException();
};

struct MetaInfo {
    static const char* GetName(TypeInfo ti);
    static uint32_t GetTypeSize(TypeInfo ti);
    static uint8_t GetAlign(TypeInfo ti);

    static bool IsReferenceType(TypeInfo ti);

    // Visits offsets of reference fields in GCTib.
    // NOTE: offsets are relative to object/struct start address.
    static void VisitReferences(TypeInfo ti, std::function<void(uint32_t)> visitor);

    static uint32_t ObjectHeaderSize() { return sizeof(void*); }

    static uint32_t ArrayBodyOffset() { return sizeof(void*) + sizeof(uint64_t); }

    static TypeInfo ByteArrayTypeInfo();
};

} // namespace RTSupport
