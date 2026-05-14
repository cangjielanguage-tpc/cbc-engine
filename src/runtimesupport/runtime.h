#pragma once

/// This file defines Runtime specific interface for communication between
/// interpreter and the runtime.

#include "interpreter/ectype.h"
#include <cstdint>
#include <optional>

namespace RTSupport {

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

    static void* GcPoint();

    static void* GcPointTrampoline();

    static bool IsPendingSafePoint();

    static Reference NewArray(TypeInfo type, size_t count, ThreadHandle th);
    static size_t ArrayLength(Reference array);

    static Reference ReadObjectInstance(Reference base, size_t offset, ThreadHandle th);
    static void WriteObjectInstance(Reference base, size_t offset, Reference object, ThreadHandle th);
    static Reference ReadObjectStatic(void* location, ThreadHandle th);
    static void WriteObjectStatic(void* location, Reference object, ThreadHandle th);

    static TypeInfo GetTypeInfo(Reference base);

    static void* GetVirtualTarget(Reference base, int extDefNum, int methodNum);
    static void* GetInterfaceTarget(Reference base, TypeInfo ti, int methodNum);

    static int GetFieldOffset(TypeInfo ti, int ordinal, bool adjustByHeader);
};

struct MetaInfo {
    static uint32_t GetTypeSize(TypeInfo ti);
    static uint8_t GetAlign(TypeInfo ti);
};

} // namespace RTSupport
