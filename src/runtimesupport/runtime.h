#pragma once

/// This file defines Runtime specific interface for communication between
/// interpreter and the runtime.

#include "interpreter/ectype.h"

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

class MethodTable {
public:
    explicit MethodTable(uintptr_t _value) : value(reinterpret_cast<void*>(_value)) {}

    explicit MethodTable(void* value) : value(value) {}

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

    static Reference NewArray(TypeInfo type, size_t count, ThreadHandle th);
    static size_t ArrayLength(Reference array);

    static Reference ReadObjectInstance(Reference base, size_t offset, ThreadHandle th);
    static void WriteObjectInstance(Reference base, size_t offset, Reference object, ThreadHandle th);
    static Reference ReadObjectStatic(void* location, ThreadHandle th);
    static void WriteObjectStatic(void* location, Reference object, ThreadHandle th);
    static Reference ReadObject(uintptr_t base, size_t offset, ThreadHandle th);
    static void WriteObject(uintptr_t base, size_t offset, Reference object, ThreadHandle th);

    static TypeInfo GetTypeInfo(Reference base);

    static MethodTable GetMethodTable(Reference base, int extDefNum, int methodNum);
};

struct Runtime {
    static TypeInfo GetTypeInfo(const char* typeName);

    static char const* GetTypeInfoName(TypeInfo typeInfo);
};

} // namespace RTSupport
