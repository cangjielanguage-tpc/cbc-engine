#ifndef INTERPRETER_ECTYPE_H
#define INTERPRETER_ECTYPE_H

#include "cbc/isa.h"
#include "functional"

namespace Interpretation {

using namespace Cbc;

namespace Value {
    struct Reference {
        uintptr_t value;
    };
    union Primitive {
        uint64_t u64;
        int64_t i64;
        uint32_t u32;
        int32_t i32;
        uint16_t u16;
        uint8_t u8;
    };
};

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
    Ectype() : iregs{}, iregMarks{}, fregs{} {}

    inline void Put(IReg reg, Value::Primitive primitive) {
        ASSERT(reg != IReg::IRZ);
        iregs[reg].primitive = primitive;
        iregMarks[reg] = Mark::PRIMITIVE;
    }

    inline void Put(IReg reg, Value::Reference reference) {
        ASSERT(reg != IReg::IRZ);
        iregs[reg].reference = reference;
        iregMarks[reg] = Mark::REFERENCE;
    }

    inline void Put(FReg reg, Value::Primitive primitive) {
        fregs[reg].primitive = primitive;
    }

    inline Value::Reference GetReference(IReg reg) {
        ASSERT(iregMarks[reg] == Mark::REFERENCE);
        return iregs[reg].reference;
    }

    inline Value::Primitive GetPrimitive(IReg reg) {
        ASSERT(iregMarks[reg] == Mark::PRIMITIVE);
        return iregs[reg].primitive;
    }

    inline Value::Primitive GetPrimitive(FReg reg) {
        return fregs[reg].primitive;
    }

    void VisitReferences(std::function<void(Value::Reference*)> visitor);

private:
    IRegContainer iregs[IReg::COUNT];
    Mark iregMarks[IReg::COUNT];

    FRegContainer fregs[FReg::COUNT];
};

} // namespace Interpreter

#endif // INTERPRETER_ECTYPE_H
