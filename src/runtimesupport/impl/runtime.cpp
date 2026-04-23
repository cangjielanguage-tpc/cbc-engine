#include "runtimesupport/runtime.h"
#include "asm_trampolines.h"
#include "cjnative.h"

namespace RTSupport {

using Reference = Interpretation::Value::Reference;

Reference RuntimeInterface::ReadObjectInstance(Reference base, size_t offset, ThreadHandle th)
{
    return Reference { .value = reinterpret_cast<uintptr_t>(g_CJNativeInterfaceInstance.read_instance_field(
                           reinterpret_cast<MRTExport::obj_ref_t>(base.value),
                           reinterpret_cast<MRTExport::field_ref_t>(base.value + offset)
                       )) };
}

void RuntimeInterface::WriteObjectInstance(Reference base, size_t offset, Reference object, ThreadHandle th)
{
    g_CJNativeInterfaceInstance.write_instance_field(
        reinterpret_cast<MRTExport::obj_ref_t>(base.value),
        reinterpret_cast<MRTExport::field_ref_t>(base.value + offset),
        reinterpret_cast<MRTExport::obj_ref_t>(object.value)
    );
}

Reference RuntimeInterface::ReadObject(uintptr_t base, size_t offset, ThreadHandle th)
{
    return Reference { .value = reinterpret_cast<uintptr_t>(g_CJNativeInterfaceInstance.read_static_field(
                           reinterpret_cast<MRTExport::field_ref_t>(base + offset)
                       )) };
}

void RuntimeInterface::WriteObject(uintptr_t base, size_t offset, Reference object, ThreadHandle th)
{
    g_CJNativeInterfaceInstance.write_static_field(
        reinterpret_cast<MRTExport::field_ref_t>(base + offset), reinterpret_cast<MRTExport::obj_ref_t>(object.value)
    );
}

TypeInfo RuntimeInterface::GetTypeInfo(const char* typeName) { return g_CJNativeInterfaceInstance.type_info(typeName); }

void* RuntimeInterface::AllocateObjectInstance() { return reinterpret_cast<void*>(&Asm::engine_i2_newobject); }

} // namespace RTSupport
