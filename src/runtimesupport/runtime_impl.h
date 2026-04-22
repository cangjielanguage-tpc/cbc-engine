#pragma once

#include "cbc/dispatcher_rt.h"

namespace RTSupport {

struct Impl {};

template <> class RuntimeInterface<Impl> {
    using Reference = Interpretation::Value::Reference;

public:
    inline static void* AllocateObject;

    static Reference ReadObjectInstance(Reference base, size_t offset, ThreadHandle th);

    static void WriteObjectInstance(Reference base, size_t offset, Reference object, ThreadHandle th);

    static Reference ReadObjectStatic(void* location, ThreadHandle th);

    static void WriteObjectStatic(void* location, Reference object, ThreadHandle th);

    static TypeInfo<Impl> GetTypeInfo(const char* typeName);
};

void InitializeRuntimeInterface();

} // namespace RTSupport
