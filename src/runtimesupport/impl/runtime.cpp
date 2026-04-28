#include "runtimesupport/runtime.h"
#include "RuntimeTypes.h"
#include "asm_trampolines.h"
#include "cjnative.h"
#include "utils/assertion.h"

namespace RTSupport {

using Reference = Interpretation::Value::Reference;

static MRTExport::type_info_t* MRTTypeInfo(TypeInfo typeInfo)
{
    return reinterpret_cast<MRTExport::type_info_t*>(typeInfo.Raw());
}

Reference Execution::ReadObjectInstance(Reference base, size_t offset, ThreadHandle th)
{
    return Reference { .value = reinterpret_cast<uintptr_t>(g_CJNativeInterfaceInstance.read_instance_field(
                           reinterpret_cast<MRTExport::obj_ref_t>(base.value),
                           reinterpret_cast<MRTExport::field_ref_t>(base.value + offset)
                       )) };
}

void Execution::WriteObjectInstance(Reference base, size_t offset, Reference object, ThreadHandle th)
{
    g_CJNativeInterfaceInstance.write_instance_field(
        reinterpret_cast<MRTExport::obj_ref_t>(base.value),
        reinterpret_cast<MRTExport::field_ref_t>(base.value + offset),
        reinterpret_cast<MRTExport::obj_ref_t>(object.value)
    );
}

Reference Execution::ReadObjectStatic(void* location, ThreadHandle th)
{
    return Reference { .value = reinterpret_cast<uintptr_t>(g_CJNativeInterfaceInstance.read_static_field(
                           reinterpret_cast<MRTExport::field_ref_t>(location)
                       )) };
}

void Execution::WriteObjectStatic(void* location, Reference object, ThreadHandle th)
{
    g_CJNativeInterfaceInstance.write_static_field(
        reinterpret_cast<MRTExport::field_ref_t>(location), reinterpret_cast<MRTExport::obj_ref_t>(object.value)
    );
}

void* Execution::AllocateObjectInstance() { return reinterpret_cast<void*>(&Asm::engine_i2_newobject); }

TypeInfo Execution::GetTypeInfo(Reference base)
{
    TypeInfo* header = reinterpret_cast<TypeInfo*>(base.value);
    return *header;
}

MethodTable Execution::GetMethodTable(Reference base, int extDefNum, int methodNum)
{
    MRTExport::type_info_t** header = reinterpret_cast<MRTExport::type_info_t**>(base.value);
    auto typeInfo                   = *header;
    auto target                     = typeInfo->v_extension_data_start[extDefNum]->func_table[methodNum];
    return MethodTable(target);
}

int Execution::GetFieldOffset(TypeInfo ti, int ordinal)
{
    auto mrtti = MRTTypeInfo(ti);
    ASSERT(ordinal < mrtti->field_num);
    return mrtti->field_offsets[ordinal];
}

} // namespace RTSupport
