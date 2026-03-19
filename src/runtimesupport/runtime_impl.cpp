#include "cbc/dispatcher_rt.h"
#include "cbc_engine.h"
#include "cjnative.h"
#include "interpreter/runtime.h"

extern "C" { // exported to ASM
void* (*engine_newobject_function)(void*);
}

extern "C" { // declared in ASM
void engine_i2_newobject_0();
void engine_i2_newobject_1();
void engine_i2_newobject_2();
void engine_i2_newobject_3();
void engine_i2_newobject_4();
void engine_i2_newobject_5();
void engine_i2_newobject_6();
void engine_i2_newobject_7();
void engine_i2_newobject_8();
void engine_i2_newobject_9();
void engine_i2_newobject_10();
void engine_i2_newobject_11();
void engine_i2_newobject_12();
void engine_i2_newobject_13();
}

namespace Interpretation {

struct Impl {};

// FIXME: real implementation
template <> class RuntimeInterface<Impl> {
    using Reference = Value::Reference;

public:
    inline static void* AllocateObject[IReg::COUNT];

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
};

void InitializeRuntimeInterface()
{
    engine_newobject_function                  = g_CJNativeInterfaceInstance.object_alloc;
    RuntimeInterface<Impl>::AllocateObject[0]  = reinterpret_cast<void*>(&engine_i2_newobject_0);
    RuntimeInterface<Impl>::AllocateObject[1]  = reinterpret_cast<void*>(&engine_i2_newobject_1);
    RuntimeInterface<Impl>::AllocateObject[2]  = reinterpret_cast<void*>(&engine_i2_newobject_2);
    RuntimeInterface<Impl>::AllocateObject[3]  = reinterpret_cast<void*>(&engine_i2_newobject_3);
    RuntimeInterface<Impl>::AllocateObject[4]  = reinterpret_cast<void*>(&engine_i2_newobject_4);
    RuntimeInterface<Impl>::AllocateObject[5]  = reinterpret_cast<void*>(&engine_i2_newobject_5);
    RuntimeInterface<Impl>::AllocateObject[6]  = reinterpret_cast<void*>(&engine_i2_newobject_6);
    RuntimeInterface<Impl>::AllocateObject[7]  = reinterpret_cast<void*>(&engine_i2_newobject_7);
    RuntimeInterface<Impl>::AllocateObject[8]  = reinterpret_cast<void*>(&engine_i2_newobject_8);
    RuntimeInterface<Impl>::AllocateObject[9]  = reinterpret_cast<void*>(&engine_i2_newobject_9);
    RuntimeInterface<Impl>::AllocateObject[10] = reinterpret_cast<void*>(&engine_i2_newobject_10);
    RuntimeInterface<Impl>::AllocateObject[11] = reinterpret_cast<void*>(&engine_i2_newobject_11);
    RuntimeInterface<Impl>::AllocateObject[12] = reinterpret_cast<void*>(&engine_i2_newobject_12);
    RuntimeInterface<Impl>::AllocateObject[13] = reinterpret_cast<void*>(&engine_i2_newobject_13);
}

} // namespace Interpretation

namespace Cbc::RT {
using namespace Interpretation;
template __attribute__((used)) Thunk InterpretationLoop<Impl>(
    Ectype* ectype, Frame* frame, ThreadHandle handle, LiteralTable* literals, Decoder::ByteReader& reader0
);
} // namespace Cbc::RT

extern "C" {
void engine_interpretation_loop() __attribute__((alias(
    "_ZN3Cbc2RT18InterpretationLoopIN14Interpretation4ImplEEENS0_5ThunkEPNS2_6EctypeEPNS2_5FrameENS2_"
    "12ThreadHandleEPNS2_12LiteralTableERN7Decoder10ByteReaderE"
)));
} // extern "C"
