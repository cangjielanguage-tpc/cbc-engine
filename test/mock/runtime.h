#pragma once

#include "runtimesupport/runtime.h"

namespace RTSupport {

struct Test {};

template <> class RuntimeInterface<Test> {
    using Reference = Interpretation::Value::Reference;

public:
    inline static void* AllocateObject;

    static Reference ReadObjectInstance(Reference base, size_t offset, ThreadHandle th)
    {
        return Reference { .value = *reinterpret_cast<uintptr_t*>(base.value + offset) };
    }

    static void WriteObjectInstance(Reference base, size_t offset, Reference object, ThreadHandle th)
    {
        *reinterpret_cast<uintptr_t*>(base.value + offset) = object.value;
    }

    static Reference ReadObject(uintptr_t base, size_t offset, ThreadHandle th)
    {
        return Reference { .value = *reinterpret_cast<uintptr_t*>(base + offset) };
    }

    static void WriteObject(uintptr_t base, size_t offset, Reference object, ThreadHandle th)
    {
        *reinterpret_cast<uintptr_t*>(base + offset) = object.value;
    }

    static TypeInfo<Test> GetTypeInfo(const char *typeName) 
    {
        return TypeInfo<Test>(nullptr);
    }
};

} // namespace RTSupport