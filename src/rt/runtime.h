#ifndef RT_RUNTIME_H
#define RT_RUNTIME_H

/// This file defines Runtime specific interface for communication between
/// interpreter and the runtime.

#include "interpreter/ectype.h"

namespace Runtime {

template <typename RT>
class TypeInfo;

template <typename RT>
class ThreadHandle;

template <typename RT>
class RuntimeInterface : public RT {
    using Reference = Interpretation::Value::Reference;
public:
    Reference NewObj(TypeInfo<RT> type, ThreadHandle<RT> th);
    Reference NewArray(TypeInfo<RT> type, size_t count, ThreadHandle<RT> th);
    size_t ArrayLength(Reference array);

    Reference ReadObjectInstance(Reference base, size_t offset, ThreadHandle<RT> th);
    void WriteObjectInstance(Reference base, size_t offset, Reference object, ThreadHandle<RT> th);
    Reference ReadObjectStatic(void* location, ThreadHandle<RT> th);
    void WriteObjectStatic(void* location, Reference object, ThreadHandle<RT> th);

    Interpretation::Ectype* GetEctype(ThreadHandle<RT> th);
};

}
#endif // RT_RUNTIME_H
