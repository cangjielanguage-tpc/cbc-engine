#ifndef INTERPRETATION_RUNTIME_H
#define INTERPRETATION_RUNTIME_H

/// This file defines Runtime specific interface for communication between
/// interpreter and the runtime.

#include "interpreter/ectype.h"

namespace Interpretation {

class ThreadHandle {
public:
    ThreadHandle(void* _value) : value(_value) {}

    operator void*() const { return value; }

private:
    void* const value;
};

template <typename RT> class TypeInfo {
public:
    TypeInfo(uintptr_t _value) : value((void*)_value) {}

    TypeInfo(void* _value) : value(_value) {}

    operator void*() const { return value; }

private:
    void* const value;
};

template <typename RT> class RuntimeInterface {
    using Reference = Value::Reference;

public:
    /// Each element represent an function that accepts (Ectype, ThreadHandle, TypeInfo)
    /// and puts result in IReg(idx) register.
    ///
    /// This specialization is needed to allow Thunk usage.
    inline static void* AllocateObject[IReg::COUNT];

    static Reference NewArray(TypeInfo<RT> type, size_t count, ThreadHandle th);
    static size_t ArrayLength(Reference array);

    static Reference ReadObjectInstance(Reference base, size_t offset, ThreadHandle th);
    static void WriteObjectInstance(Reference base, size_t offset, Reference object, ThreadHandle th);
    static Reference ReadObjectStatic(void* location, ThreadHandle th);
    static void WriteObjectStatic(void* location, Reference object, ThreadHandle th);
    static Reference ReadObject(uintptr_t base, size_t offset, ThreadHandle th);
    static void WriteObject(uintptr_t base, size_t offset, Reference object, ThreadHandle th);

    static Ectype* GetEctype(ThreadHandle th);
};

} // namespace Interpretation
#endif // INTERPRETATION_RUNTIME_H
