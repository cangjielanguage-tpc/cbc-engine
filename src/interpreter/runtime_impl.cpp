#include "cbc/dispatcher_rt.h"
#include "runtime.h"

namespace Interpretation {

struct Impl {};

// FIXME: real implementation
template <> class RuntimeInterface<Impl> {
    using Reference = Value::Reference;

public:
    inline static Reference NewObj(TypeInfo<Impl> type, ThreadHandle th)
    {
        return Value::Reference { .value = reinterpret_cast<uintptr_t>(nullptr) };
    }

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

} // namespace Interpretation

namespace Cbc::RT {
using namespace Interpretation;
template __attribute__((used)) void InterpretationLoop<Impl>(
    Ectype* ectype, Frame* frame, ThreadHandle handle, LiteralTable* literals, Decoder::ByteReader& reader0
);
} // namespace Cbc::RT

extern "C" {
void engine_interpretation_loop() __attribute__((alias(
    "_ZN3Cbc2RT18InterpretationLoopIN14Interpretation4ImplEEEvPNS2_6EctypeEPNS2_5FrameENS2_12ThreadHandleEPNS2_"
    "12LiteralTableERN7Decoder10ByteReaderE"
)));
} // extern "C"
