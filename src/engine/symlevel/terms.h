#pragma once

#include "engine/engine.h"
#include "io/file_id.h"
#include "offset.h"
#include "utils/assertion.h"
#include "utils/math.h"
#include <cstdint>

namespace Symlevel {

enum class TemplateKind : uint16_t {
    // primitives start
    NIL,
    VOID,
    UNIT,
    NOTHING,
    BOOLEAN,
    I8,
    U8,
    I16,
    U16,
    I32,
    U32,
    UCHAR32,
    I64,
    U64,
    IADDR,
    UADDR,
    BSTRING,
    F16,
    F32,
    F64,
    // primitives end

    // builtin types start
    C_POINTER,
    NULLABLE,
    NON_NULLABLE,
    CANGJIE_ARRAY,
    METHOD,
    // builtin types end

    TYPE,
    AOT_TYPE,
    TYPE_VAR,
    GENERIC_METHOD,
    LAST
};

static constexpr auto FIRST_NON_PRIMITIVE = static_cast<uint16_t>(TemplateKind::C_POINTER);

class TemplateIdentifier {
    static constexpr uint64_t KIND_MASK = 0xFFFFlu;
    static constexpr uint64_t NUM_MASK  = ~KIND_MASK;

public:
    TemplateIdentifier(TemplateKind kind, uint64_t num) : raw(static_cast<uint64_t>(kind) | (num << 16))
    {
        ASSERT(kind < TemplateKind::LAST);
        ASSERT(MathUtils::IsNBits(num, 48));
    }

    TemplateIdentifier(TemplateKind kind) : TemplateIdentifier(kind, 0) {}

    TemplateKind GetKind() { return TemplateKind(raw & KIND_MASK); }

    uint64_t GetNum() { return (raw & NUM_MASK) >> 16; }

private:
    uint64_t raw;
};

class Term;
class LocalTerm;
class GlobalTerm;
struct TermData;

class LocalTerm {
public:
    Term Subterm(uint32_t i) const;

    // TODO: implement
    // GlobalTerm Publish(Engine::Session& session);

private:
    friend class Term;

    LocalTerm(TermData* data) : data(data) {};
    TermData* data;
};

class GlobalTerm {
public:
    GlobalTerm Subterm(uint32_t i) const;

private:
    friend class Term;

    GlobalTerm(TermData* data) : data(data) {};
    TermData* data;
};

class Term {
public:
    static std::optional<Term> ParseAndResolve(Engine::Session& session, IO::FileId fileId, Offset<Term> offset);
    static Term Builtin(Engine::Session& session, TemplateKind kind);

    TemplateIdentifier GetIdentifier() const;
    uint32_t GetLength() const;

    bool IsLocal() const;
    LocalTerm AsLocal();
    GlobalTerm AsGlobal();

private:
    Term(LocalTerm local) : data(local.data) {}

    Term(GlobalTerm global) : data(global.data) {}

    TermData* data;
};

static constexpr bool IsBuiltin(uint32_t idx) { return idx < FIRST_NON_PRIMITIVE; }

} // namespace Symlevel
