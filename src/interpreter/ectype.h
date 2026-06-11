#ifndef INTERPRETER_ECTYPE_H
#define INTERPRETER_ECTYPE_H

#include "asm_export.h"
#include "cbc/isa.h"
#include "interpreter/loggers.h"
#include <cstddef>

#define SERVICE_REGS_COUNT 1
#define MAGIC_WORD 0xCBC0C0DE

namespace Interpretation {

using namespace Cbc;

namespace Value {
struct Reference {
    uintptr_t value;
};

/// Type-punning using union is not allowed in C++ as in C.
/// So, adding other values here, like i32 or i64 can cause bugs.
/// Instead, we need to cast values.
union Primitive {
    uint64_t u64;
    uint32_t u32;
    float f32;
    double f64;
};

static_assert(sizeof(float) == 4);
static_assert(sizeof(double) == 8);
}; // namespace Value

union IRegContainer {
    Value::Primitive primitive;
    Value::Reference reference;
};

struct FRegContainer {
    Value::Primitive primitive;
};

union SRegContainer {
    Value::Primitive primitive;
};

enum class Mark : uint8_t {
    PRIMITIVE = 0,
    REFERENCE = 1
};

class Ectype {
public:
    // zero-initialize everything (including marks)
    Ectype() : iregs {}, fregs {}, sregs {} {}

    inline void Put(IReg reg, Value::Primitive primitive)
    {
#ifndef NDEBUG
        Log::interpretation.Log(Logging::Level::TRACE, [&](Stream::Output& out) {
            out << reg.ToStr() << " <- " << primitive.u64 << Stream::endl;
        });
#endif
        ASSERT(reg != IReg::IRZ);
        iregs[reg].primitive = primitive;
    }

    inline void Put(IReg reg, Value::Reference reference)
    {
#ifndef NDEBUG
        Log::interpretation.Log(Logging::Level::TRACE, [&](Stream::Output& out) {
            out.PrintFmt("%s <- %p", reg.ToStr().data(), reference.value);
            out.NewLine();
        });
#endif
        ASSERT(reg != IReg::IRZ);
        iregs[reg].reference = reference;
    }

    inline void Put(FReg reg, Value::Primitive primitive)
    {
#ifndef NDEBUG
        Log::interpretation.Log(Logging::Level::TRACE, [&](Stream::Output& out) {
            out << reg.ToStr() << " <- " << primitive.f64 << Stream::endl;
        });
#endif
        fregs[reg].primitive = primitive;
    }

    inline void PutSReg(int reg, Value::Primitive primitive)
    {
        ASSERT(reg < SERVICE_REGS_COUNT);
        sregs[reg].primitive = primitive;
    }

    inline Value::Primitive GetSReg(int reg)
    {
        ASSERT(reg < SERVICE_REGS_COUNT);
        return sregs[reg].primitive;
    }

    inline Value::Reference GetReference(IReg reg) { return iregs[reg].reference; }

    inline Value::Primitive GetPrimitive(IReg reg) { return iregs[reg].primitive; }

    inline Value::Primitive GetPrimitive(FReg reg) { return fregs[reg].primitive; }

    inline IRegContainer* GetIRegLocation(IReg reg) { return &iregs[reg]; }

    inline Ectype* Checked()
    {
        ASSERT(magic == MAGIC_WORD);
        return this;
    }

private:
    friend class EctypeInvariants;
    IRegContainer iregs[IReg::COUNT];
    FRegContainer fregs[FReg::COUNT];

    /// Service registers can be used for internal interpreter operations (e.g. for exception handling)
    /// and should not be reachable from CBC bytecode. Do not put traceable values here.
    SRegContainer sregs[SERVICE_REGS_COUNT];

    uint32_t magic = MAGIC_WORD;
};

class EctypeInvariants {
    static_assert(offsetof(Ectype, iregs) == ECTYPE_IREGS_OFFSET);
    static_assert(offsetof(Ectype, fregs) == ECTYPE_FREGS_OFFSET);
    static_assert(offsetof(Ectype, sregs) == ECTYPE_SREGS_OFFSET);
};

} // namespace Interpretation

#endif // INTERPRETER_ECTYPE_H
