#pragma once

/// This file defines Runtime specific interface for communication between
/// interpreter and the runtime.

#include "interpreter/ectype.h"
#include <cstdint>
#include <functional>
#include <optional>

namespace RTSupport {

// TypeInfo flags
static constexpr uint8_t HAS_REF_FIELD = 0b00000001;

static constexpr uint64_t GCTIB_SIGN_BIT = (1lu << 63);
static constexpr uint32_t GCTIB_MAX_SHORT_OFFSET = sizeof(void*) * 62;

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

    static void* GcPoint();

    static void* GcPointTrampoline();

    static bool IsPendingSafePoint();

    static size_t ArrayLength(Reference array);

    static Reference ReadObjectInstance(Reference base, size_t offset, ThreadHandle th);
    static void WriteObjectInstance(Reference base, size_t offset, Reference object, ThreadHandle th);
    static Reference ReadObjectStatic(void* location, ThreadHandle th);
    static void WriteObjectStatic(void* location, Reference object, ThreadHandle th);

    static TypeInfo GetTypeInfo(Reference base);

    static void* GetVirtualTarget(Reference base, int extDefNum, int methodNum);
    static void* GetInterfaceTarget(Reference base, TypeInfo ti, int methodNum);

    static int GetFieldOffset(TypeInfo ti, int ordinal, bool adjustByHeader);

    static bool IsInstanceOf(Reference base, TypeInfo ti);

    // Visits offsets of reference fields in GCTib.
    // NOTE: offsets are relative to object/struct body (NO HEADER)!
    static void VisitReferences(TypeInfo ti, std::function<void(uint32_t)> visitor);
};

struct MetaInfo {
    static uint32_t GetTypeSize(TypeInfo ti);
    static uint8_t GetAlign(TypeInfo ti);

    static bool IsReferenceType(TypeInfo ti);

    static void VisitReferences(TypeInfo ti, std::function<void(uint32_t)> visitor);

    static uint32_t ObjectHeaderSize() { return sizeof(void*); }

    static TypeInfo ByteArrayTypeInfo();
};

} // namespace RTSupport
