#pragma once

/// This file defines Runtime specific interface for communication between
/// interpreter and the runtime.

#include "RuntimeTypes.h"
#include "interpreter/ectype.h"

namespace RTSupport {

class ThreadHandle {
public:
    ThreadHandle(void* _value) : value(_value) {}

    operator void*() const { return value; }

private:
    void* const value;
};

template <typename RT> class TypeInfo {
public:
    TypeInfo() : value(nullptr) {};

    TypeInfo(uintptr_t _value) : value((MRTExport::type_info_t*)_value) {}

    TypeInfo(MRTExport::type_info_t* _value) : value(_value) {}

    operator void*() const { return value; }

    operator MRTExport::type_info_t*() { return value; }

private:
    MRTExport::type_info_t* value;
};

template <typename RT> class RuntimeInterface {
    using Reference = Interpretation::Value::Reference;

public:
    /// Each element represent an function that accepts (Ectype, ThreadHandle, TypeInfo)
    /// and puts result in IReg(idx) register.
    ///
    /// This specialization is needed to allow Thunk usage.
    inline static void* AllocateObject;

    static Reference NewArray(TypeInfo<RT> type, size_t count, ThreadHandle th);
    static size_t ArrayLength(Reference array);

    static Reference ReadObjectInstance(Reference base, size_t offset, ThreadHandle th);
    static void WriteObjectInstance(Reference base, size_t offset, Reference object, ThreadHandle th);
    static Reference ReadObjectStatic(void* location, ThreadHandle th);
    static void WriteObjectStatic(void* location, Reference object, ThreadHandle th);

    static TypeInfo<RT> GetTypeInfo(const char* typeName);
};

} // namespace RTSupport
