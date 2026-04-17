#pragma once

#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/symlevel/definitions.h"
#include "io/file_id.h"
#include "offset.h"
#include "string.h"
#include "utils/assertion.h"
#include "utils/math.h"
#include <cstdint>
#include <mutex>
#include <unordered_set>

/// `Term` is an symbolic representation of any type that is supported in CBC.
/// It can represent primitives (e.g. I32), builtins (e.g. ARRAY) or user-defined types (e.g. TYPE).
///
/// Each term is represented as pointer to the structure:
/// ```c
/// struct TermData {
///     TemplateIdentifier identifier;
///     uint32_t hash;
///     uint16_t length;
///     bool isLocal;
///     Term subterms[];
/// };
/// ```
/// Depending on the location, where this structure is allocated, the term could be "global" or "local".
///
/// Local terms are being created as part of `Session` in its arena, while global terms are stored permanently.
/// To reduce memory usage, global terms are interned.
///
/// The most important part of Term structure is `identifier` and `subterms[]`.
/// The identifier consists of two parts: `kind` and `num`. Most of the times, `kind` is representing
/// builtin type, so `num` part is not needed, but in case of user defined types,
/// `num` stores information that identifies the type being referefenced.
namespace Symlevel {

// Custom identifiers
using AotTypeIdentifier = Engine::Identifier<String>;

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

    TemplateIdentifier(AotTypeIdentifier identifier) : TemplateIdentifier(TemplateKind::AOT_TYPE, identifier) {}

    TemplateIdentifier(TemplateKind kind, Offset<String> offset, IO::FileId fileId) {}

    TemplateIdentifier(TemplateKind kind) : TemplateIdentifier(kind, 0) {}

    TemplateKind GetKind() { return TemplateKind(raw & KIND_MASK); }

    uint64_t GetNum() { return (raw & NUM_MASK) >> 16; }

    uint32_t Hash() { return static_cast<uint32_t>(raw ^ (raw >> 32)); }

    bool operator==(const TemplateIdentifier& another) const { return raw == another.raw; }

    bool operator!=(const TemplateIdentifier& another) const { return raw != another.raw; }

    AotTypeIdentifier AsAotType()
    {
        ASSERTION(GetKind() == TemplateKind::AOT_TYPE, "aot type kind expected");
        return AotTypeIdentifier(GetNum());
    }

private:
    uint64_t raw;
};

class Term;
class LocalTerm;
class GlobalTerm;
struct TermData;

class LocalTerm {
public:
    LocalTerm(TermData* data);
    Term Subterm(uint32_t i) const;

    GlobalTerm Publish(Engine::Session& session);

private:
    friend class Term;
    TermData* data;
};

class GlobalTerm {
public:
    GlobalTerm(TermData* data) : data(data) {}
    GlobalTerm Subterm(uint32_t i) const;

private:
    friend class Term;
    TermData* data;
};

struct Term {
    TermData* data;

    static std::optional<Term> ParseAndResolve(Engine::Session& session, IO::FileId fileId, Offset<Term> offset);
    static Term Primitive(Engine::Session& session, TemplateKind kind);

    static Term Definition(Engine::Session& session, Engine::Identifier<TypeDefinition> type);

    Term(LocalTerm local) : data(local.data) {}

    Term(GlobalTerm global) : data(global.data) {}

    TemplateIdentifier GetIdentifier() const;
    uint32_t GetLength() const;
    uint32_t Hash() const;

    bool IsLocal() const;
    LocalTerm AsLocal();
    GlobalTerm AsGlobal();

    bool operator==(const Term& another) const;
    bool operator!=(const Term& another) const;

    Term Subterm(uint32_t i) const;
};

static constexpr bool IsBuiltin(uint32_t idx) { return idx < FIRST_NON_PRIMITIVE; }

class TermManager {
public:
    class Impl;
    friend class Impl;

    static TermManager& Of(Engine::Engine& engine);
    static TermManager& Of(Engine::Session& session);

    /// Globalize given term.
    /// The function performs in-place modification of `Term` structure.
    GlobalTerm Globalize(Term& term);

private:
    struct Hasher {
        uint64_t operator()(TermData* const& data) const;
    };

    std::mutex lock;
    std::unordered_set<TermData*, Hasher> cache;
};

} // namespace Symlevel
