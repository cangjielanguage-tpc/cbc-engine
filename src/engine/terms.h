#pragma once

#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/symlevel/definitions.h"
#include "utils/assertion.h"
#include "utils/ostream.h"
#include "utils/reinterpretation.h"
#include <cstdint>
#include <mutex>
#include <unordered_set>

/// `Term` is an symbolic representation of any type that is supported in CBC.
/// It can represent primitives (e.g. I32), builtins (e.g. ARRAY) or user-defined types (e.g. TYPE).
///
/// Each term is represented as pointer to the structure:
/// ```c
/// struct TermData {
///     TermId identifier;
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
namespace Engine {

class Term;
class LocalTerm;
class GlobalTerm;
struct TermData;

enum class TermKind : uint8_t {
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

    UNDEFINED, // resolution error

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

static constexpr auto FIRST_NON_PRIMITIVE = static_cast<uint16_t>(TermKind::UNDEFINED);

class TermId {
public:

    static constexpr auto KIND_PART_BIT_SIZE = 8 * sizeof(TermKind);
    static constexpr auto INFO_PART_BIT_SIZE = 64 - KIND_PART_BIT_SIZE;

    TermKind GetKind() { return kind; }

    uint32_t Hash()
    {
        std::hash<uint64_t> hash;
        return hash(Raw());
    }

    bool operator==(TermId const& another) const { return Raw() == another.Raw(); }

    bool operator!=(TermId const& another) const { return !(*this == another); }

protected:
    constexpr TermId(TermKind kind, uint64_t info) : kind(kind), info(info)
    {
        ASSERT(info < (1lu << INFO_PART_BIT_SIZE));
    }

    uint64_t Raw() const { return Bits::Raw64(*this); }

    TermKind kind : KIND_PART_BIT_SIZE;
    uint64_t info : INFO_PART_BIT_SIZE;
};

struct TagTermId : public TermId {
    constexpr TagTermId(TermKind kind) : TermId(kind, 0) {}
    explicit constexpr TagTermId(TermId ident) : TermId(ident)
    {
        ASSERT(info == 0);
    }
};

class Term {
public:
    static constexpr uint16_t FIRST_NON_PRIMITIVE = static_cast<uint16_t>(TermKind::UNDEFINED);

    TermData* data;

    static Term Definition(Session& session, Identifier<Symlevel::TypeDefinition> type);

    Term(LocalTerm local);
    Term(GlobalTerm global);

    TermId GetIdentifier() const;
    TermKind GetKind() const;
    uint32_t GetLength() const;
    uint32_t Hash() const;

    std::string GetName(Session& session) const;
    void GetName(Session& session, Stream::Output& stream) const;

    bool IsLocal() const;
    LocalTerm AsLocal();
    GlobalTerm AsGlobal();

    bool operator==(const Term& another) const;
    bool operator!=(const Term& another) const;

    Term Subterm(uint32_t i) const;

    struct Hasher {
        uint64_t operator()(Term const& term) const { return term.Hash(); }
    };
};

class LocalTerm {
public:
    LocalTerm(TermData* data);
    Term Subterm(uint32_t i) const;

    GlobalTerm Publish(Session& session);

    TermId GetIdentifier() const { return Term(*this).GetIdentifier(); }

    uint32_t GetLength() const { return Term(*this).GetLength(); }

    uint32_t Hash() const { return Term(*this).GetLength(); }

private:
    friend class Term;
    TermData* data;
};

class GlobalTerm {
public:
    GlobalTerm(TermData* data) : data(data) {}

    GlobalTerm Subterm(uint32_t i) const;

    TermId GetIdentifier() const { return Term(*this).GetIdentifier(); }

    uint32_t GetLength() const { return Term(*this).GetLength(); }

    uint32_t Hash() const { return Term(*this).GetLength(); }

    bool operator==(const GlobalTerm& another) const;
    bool operator!=(const GlobalTerm& another) const;

private:
    friend class Term;
    TermData* data;
};

template <typename Id, TermKind tk>
struct _SpecializedTermId : public TermId {
    _SpecializedTermId(Id identifier)
        : TermId(tk, Bits::Raw64(identifier.Pack())) {}

    explicit _SpecializedTermId(Term term) : _SpecializedTermId(term.GetIdentifier()) {}

    explicit _SpecializedTermId(TermId ident) : TermId(ident) {
        ASSERT(ident.GetKind() == tk);
    }

    Id GetIdentifier()
    {
        typename Id::Packed packed{};
        uint64_t info = this->info;
        std::memcpy(&packed, &info, sizeof(packed));
        return Id(packed);
    }
};

using AotTermId = _SpecializedTermId<Identifier<Symlevel::String>, TermKind::AOT_TYPE>;
using TypeTermId = _SpecializedTermId<Identifier<Symlevel::TypeDefinition>, TermKind::TYPE>;
using UndefTermId = _SpecializedTermId<IndexIdentifier<Term>, TermKind::UNDEFINED>;

/// Term manager provides utilities for caching (and interning) of global terms,
/// and responsible for resolution of term identifiers.
class TermManager {
public:
    class Impl;
    friend class Impl;

    static TermManager& Of(Engine& engine);
    static TermManager& Of(Session& session);

    /// Perform term resolution.
    /// In case of resolution errors, UNDEFINED term will be returned.
    // TODO: pass abstract cache inside.
    static Term Resolve(Session& session, IndexIdentifier<Term> ident);

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

} // namespace Engine
