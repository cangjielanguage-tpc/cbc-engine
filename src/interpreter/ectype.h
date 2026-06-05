#ifndef INTERPRETER_ECTYPE_H
#define INTERPRETER_ECTYPE_H

#include "asm_export.h"
#include "cbc/isa.h"
#include "interpreter/loggers.h"
#include <cstddef>

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

enum class Mark : uint8_t {
    PRIMITIVE = 0,
    REFERENCE = 1
};

class Ectype {
public:
    // zero-initialize everything (including marks)
    Ectype() : iregs {}, fregs {} {}

    inline void Put(IReg reg, Value::Primitive primitive)
    {
        Log::interpretation.Log(Logging::Level::TRACE, [&](Stream::Output& out) {
            out << reg.ToStr() << " <- " << primitive.u64 << Stream::endl;
        });
        ASSERT(reg != IReg::IRZ);
        iregs[reg].primitive = primitive;
    }

    inline void Put(IReg reg, Value::Reference reference)
    {
        Log::interpretation.Log(Logging::Level::TRACE, [&](Stream::Output& out) {
            out.PrintFmt("%s <- %p", reg.ToStr().data(), reference.value);
            out.NewLine();
        });
        ASSERT(reg != IReg::IRZ);
        iregs[reg].reference = reference;
    }

    inline void Put(FReg reg, Value::Primitive primitive)
    {
        Log::interpretation.Log(Logging::Level::TRACE, [&](Stream::Output& out) {
            out << reg.ToStr() << " <- " << primitive.f64 << Stream::endl;
        });
        fregs[reg].primitive = primitive;
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

    uint32_t magic = MAGIC_WORD;
};

class EctypeInvariants {
    static_assert(offsetof(Ectype, iregs) == ECTYPE_IREGS_OFFSET);
    static_assert(offsetof(Ectype, fregs) == ECTYPE_FREGS_OFFSET);
};

} // namespace Interpretation

#endif // INTERPRETER_ECTYPE_H
