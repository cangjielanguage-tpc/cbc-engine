#include "runtime_impl.h"
#include "asm_trampolines.h"
#include "cbc_engine.h"
#include "cjnative.h"
#include "runtime.h"

namespace RTSupport {

using Reference = Interpretation::Value::Reference;

Reference RuntimeInterface<Impl>::ReadObjectInstance(Reference base, size_t offset, ThreadHandle th)
{
    return Reference { .value = reinterpret_cast<uintptr_t>(g_CJNativeInterfaceInstance.read_instance_field(
                           reinterpret_cast<MRTExport::obj_ref_t>(base.value),
                           reinterpret_cast<MRTExport::field_ref_t>(base.value + offset)
                       )) };
}

void RuntimeInterface<Impl>::WriteObjectInstance(Reference base, size_t offset, Reference object, ThreadHandle th)
{
    g_CJNativeInterfaceInstance.write_instance_field(
        reinterpret_cast<MRTExport::obj_ref_t>(base.value),
        reinterpret_cast<MRTExport::field_ref_t>(base.value + offset),
        reinterpret_cast<MRTExport::obj_ref_t>(object.value)
    );
}

Reference RuntimeInterface<Impl>::ReadObjectStatic(void* location, ThreadHandle th)
{
    return Reference { .value = reinterpret_cast<uintptr_t>(g_CJNativeInterfaceInstance.read_static_field(location)) };
}

void RuntimeInterface<Impl>::WriteObjectStatic(void* location, Reference object, ThreadHandle th)
{
    g_CJNativeInterfaceInstance.write_static_field(location, reinterpret_cast<MRTExport::obj_ref_t>(object.value));
}

TypeInfo<Impl> RuntimeInterface<Impl>::GetTypeInfo(const char* typeName)
{
    return g_CJNativeInterfaceInstance.type_info(typeName);
}

void InitializeRuntimeInterface()
{
    Asm::engine_newobject_function         = g_CJNativeInterfaceInstance.object_alloc;
    RuntimeInterface<Impl>::AllocateObject = reinterpret_cast<void*>(&Asm::engine_i2_newobject);
}

} // namespace RTSupport

namespace Cbc::RT {
using namespace RTSupport;
using namespace Interpretation;
template __attribute__((used)) Thunk InterpretationLoop<Impl>(
    Ectype* ectype, Frame* frame, ThreadHandle handle, LiteralTable* literals, Decoder::ByteReader& reader0
);
} // namespace Cbc::RT

extern "C" {
void engine_interpretation_loop() __attribute__((alias(
    "_ZN3Cbc2RT18InterpretationLoopIN9RTSupport4ImplEEENS0_5ThunkEPN14Interpretation6EctypeEPNS5_5FrameENS2_"
    "12ThreadHandleEPNS5_12LiteralTableERN7Decoder10ByteReaderE"
)));
} // extern "C"
