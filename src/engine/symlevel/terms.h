#pragma once

#include <variant>

#include "engine/engine.h"
#include "index.h"
#include "io/file_id.h"
#include "io/stream_file_reader.h"
#include "offset.h"
#include "string.h"

namespace Symlevel {
namespace Terms {

struct TemplateKind {
public:
    enum Value : uint8_t {
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
        TYPE_VAR,
        GENERIC_METHOD,
    };

    constexpr TemplateKind(uint8_t value) : value(static_cast<Value>(value)) {}

    constexpr TemplateKind(const Value value) : value(value) {}

    constexpr operator Value() const { return value; }

private:
    Value value;
};

// TODO: encode as 64-bit map to reduce size
class TemplateIdentifier {
public:
    TemplateIdentifier(TemplateKind kind, uint64_t num) : kind(kind), num(num) {}

    TemplateIdentifier(TemplateKind kind) : kind(kind), num(0) {}

    TemplateKind GetKind() { return kind; }

    uint64_t GetNum() { return num; }

private:
    TemplateKind kind;
    uint64_t num;
};

class Term;
class LocalTerm;
class GlobalTerm;
struct TermData;

class LocalTerm {
public:
    friend class Term;
    Term Subterm(uint32_t i) const;

    // TODO: implement
    // GlobalTerm Publish(Engine::Session& session);

private:
    LocalTerm(TermData* data) : data(data) {};
    TermData* data;
};

class GlobalTerm {
public:
    friend class Term;
    GlobalTerm Subterm(uint32_t i) const;

private:
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
    Term(LocalTerm local) : term(local) {}

    Term(GlobalTerm global) : term(global) {}

    std::variant<LocalTerm, GlobalTerm> term;
};

struct TermData {
    TemplateIdentifier identifier;
    uint32_t hash;
    uint32_t length;
    Term subterms[];
};

static const uint32_t RESERVED_SIZE = 20;
static_assert(RESERVED_SIZE == TemplateKind::F64 + 1);

static constexpr bool IsBuiltin(uint32_t idx) { return idx < RESERVED_SIZE; }

static constexpr uint32_t FirstNonBuiltIn() { return RESERVED_SIZE; }

} // namespace Terms
} // namespace Symlevel
