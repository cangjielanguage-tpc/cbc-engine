#pragma once

#include "engine/engine.h"
#include "engine/identifiers.h"
#include "symlevel/string.h"
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
    FUNCTIONAL,
    TUPLE,
    // builtin types end

    TYPE,
    AOT_TYPE,
    AOT_REC,
    CLASS_TYPE_VAR,
    FUNC_TYPE_VAR,
    GENERIC_METHOD,
    LAST
};

static constexpr auto FIRST_NON_PRIMITIVE = static_cast<uint16_t>(TermKind::UNDEFINED);

class TermId {
public:
    static constexpr auto KIND_PART_BIT_SIZE = 8 * sizeof(TermKind);
    static constexpr auto INFO_PART_BIT_SIZE = 64 - KIND_PART_BIT_SIZE;

    TermKind GetKind() { return kind; }

    bool IsReference();
    int Width();

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

    explicit constexpr TagTermId(TermId ident) : TermId(ident) { ASSERT(info == 0); }
};

class Term {
public:
    static constexpr uint16_t FIRST_NON_PRIMITIVE = static_cast<uint16_t>(TermKind::UNDEFINED);

    TermData* data;

    static Term Definition(Session& session, Identifier<Symlevel::TypeDefinition> type);
    static Term Predefined(TermKind tk);

    static Term ClassTypeVariable(uint8_t tv);
    static Term FuncTypeVariable(uint8_t tv);

    Term(LocalTerm local);
    Term(GlobalTerm global);
    Term(Term const& term);

    TermId GetId() const;
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
    bool IsReference() const;
    bool IsAotPromoted() const;
    bool IsGeneric() const;

    struct Hasher {
        uint64_t operator()(Term const& term) const { return term.Hash(); }
    };
};

class LocalTerm {
public:
    LocalTerm(TermData* data);
    Term Subterm(uint32_t i) const;

    GlobalTerm Publish(Session& session);

    TermId GetId() const { return Term(*this).GetId(); }

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

    TermId GetId() const { return Term(*this).GetId(); }

    uint32_t GetLength() const { return Term(*this).GetLength(); }

    uint32_t Hash() const { return Term(*this).GetLength(); }

    bool operator==(const GlobalTerm& another) const;
    bool operator!=(const GlobalTerm& another) const;

private:
    friend class Term;
    TermData* data;
};

template <typename Id, TermKind tk> struct _SpecializedTermId : public TermId {
    _SpecializedTermId(Id identifier) : TermId(tk, Bits::Raw64(identifier.Pack())) {}

    explicit _SpecializedTermId(Term term) : _SpecializedTermId(term.GetId()) {}

    explicit _SpecializedTermId(TermId ident) : TermId(ident)
    {
        ASSERTION(ident.GetKind() == tk, "expected: %d, actual: %d", tk, ident.GetKind());
    }

    Id GetIdentifier()
    {
        typename Id::Packed packed {};
        uint64_t info = this->info;
        std::memcpy(&packed, &info, sizeof(packed));
        return Id(packed);
    }
};

template <typename Num, TermKind tk> struct _NumberedTermId : public TermId {
    explicit _NumberedTermId(Num number) : TermId(tk, number) {}

    explicit _NumberedTermId(Term term) : _NumberedTermId(term.GetId()) {}

    explicit _NumberedTermId(TermId ident) : TermId(ident)
    {
        ASSERTION(ident.GetKind() == tk, "expected: %d, actual: %d", tk, ident.GetKind());
    }

    Num GetNum() { return static_cast<Num>(this->info); }
};

using ArrayTermId = _SpecializedTermId<Identifier<Symlevel::String>, TermKind::CANGJIE_ARRAY>;
using AotRefTermId = _SpecializedTermId<Identifier<Symlevel::String>, TermKind::AOT_TYPE>;
using AotRecTermId = _SpecializedTermId<Identifier<Symlevel::String>, TermKind::AOT_REC>;
using TypeTermId   = _SpecializedTermId<Identifier<Symlevel::TypeDefinition>, TermKind::TYPE>;
using UndefTermId  = _SpecializedTermId<RefIdentifier<Term>, TermKind::UNDEFINED>;

using ClassTvTermId = _NumberedTermId<uint8_t, TermKind::CLASS_TYPE_VAR>;
using FuncTvTermId  = _NumberedTermId<uint8_t, TermKind::FUNC_TYPE_VAR>;

/// Routine that substitutes terms in places of type variables.
/// To perform an substitution a mapping `TV -> Term` is required.
/// The form of mapping is represented by different implementations.
class Substitution {
public:
    Substitution(Session& session);
    virtual ~Substitution() = default;

    Term Substitute(Term term);

    inline Term operator()(Term term) { return Substitute(term); }

protected:
    virtual Term SubstituteClassTv(uint8_t typeVar) = 0;
    virtual Term SubstituteFuncTv(uint8_t typeVar)  = 0;
    Session& session;
};

/// Routine that substitutes class type variables with corresponding subterms of `term`.
/// Function type vars are mapped to themselves.
class ClassSubstitution : public Substitution {
public:
    ClassSubstitution(Session& session, Term term);

protected:
    Term SubstituteClassTv(uint8_t typeVar) override;
    Term SubstituteFuncTv(uint8_t typeVar) override;

private:
    Term term;
};

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
    static Term Resolve(Session& session, RefIdentifier<Term> ident);

    /// Globalize given term.
    /// The function performs in-place modification of `Term` structure.
    GlobalTerm Globalize(Term& term);

private:
    struct Hasher {
        uint64_t operator()(TermData* const& data) const;
    };

    struct Comparator {
        bool operator()(TermData* const& left, TermData* const& right) const;
    };

    std::mutex lock;
    std::unordered_set<TermData*, Hasher, Comparator> cache;
};

} // namespace Engine
