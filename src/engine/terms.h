#pragma once

#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/packed_identifier.h"
#include "engine/symlevel/definitions.h"
#include "symlevel/io/file_id.h"
#include "symlevel/offset.h"
#include "utils/assertion.h"
#include "utils/ostream.h"
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
namespace Engine {

class Term;
class LocalTerm;
class GlobalTerm;
struct TermData;

enum class TemplateKind : uint8_t {
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

static constexpr auto FIRST_NON_PRIMITIVE = static_cast<uint16_t>(TemplateKind::UNDEFINED);

struct AotTypeTemplateIdentifier;
struct TagTemplateIdentifier;
struct TypeTemplateIdentifier;
struct UndefinedTemplateIdentifier;

class TemplateIdentifier {
protected:
    constexpr TemplateIdentifier(PackedIdentifier identifier) : ident(identifier) {}

public:
    TemplateKind GetKind() { return TemplateKind(ident.GetTag()); }

    std::optional<const char*> GetKindName();

    uint32_t Hash()
    {
        auto v = static_cast<uint64_t>(ident);
        return (v >> 32) ^ v;
    }

    bool operator==(const TemplateIdentifier& another) const { return ident == another.ident; }

    bool operator!=(const TemplateIdentifier& another) const { return ident != another.ident; }

    TypeTemplateIdentifier AsTypeIdent();
    AotTypeTemplateIdentifier AsAotIdent();
    TagTemplateIdentifier AsTagIdent();
    UndefinedTemplateIdentifier AsUndefinedIdent();

protected:
    PackedIdentifier ident;
};

struct TagTemplateIdentifier : public TemplateIdentifier {
    constexpr TagTemplateIdentifier(TemplateKind kind)
        : TemplateIdentifier(PackedIdentifier(static_cast<uint8_t>(kind), 0, 0))
    {
        ASSERT(kind == GetKind());
    }

    friend class TemplateIdentifier;

protected:
    TagTemplateIdentifier(PackedIdentifier identifier) : TemplateIdentifier(identifier) {}
};

struct AotTypeTemplateIdentifier : public TemplateIdentifier {
    AotTypeTemplateIdentifier(Symlevel::Offset<Symlevel::String> offs, IO::FileId file)
        : TemplateIdentifier(PackedIdentifier(static_cast<uint8_t>(TemplateKind::AOT_TYPE), offs, file))
    {}

    Symlevel::Offset<Symlevel::String> GetOffset() { return TemplateIdentifier::ident.GetHigh(); }

    IO::FileId GetFile() { return TemplateIdentifier::ident.GetLow(); }

    friend class TemplateIdentifier;

protected:
    AotTypeTemplateIdentifier(PackedIdentifier identifier) : TemplateIdentifier(identifier) {}
};

struct TypeTemplateIdentifier : public TemplateIdentifier {
    TypeTemplateIdentifier(Symlevel::Offset<Symlevel::TypeDefinition> offs, IO::FileId file)
        : TemplateIdentifier(PackedIdentifier(static_cast<uint8_t>(TemplateKind::TYPE), offs, file))
    {}

    TypeTemplateIdentifier(Identifier<Symlevel::TypeDefinition> type)
        : TypeTemplateIdentifier(type.GetOffset(), type.GetFileId())
    {}

    Symlevel::Offset<Symlevel::TypeDefinition> GetOffset() { return TemplateIdentifier::ident.GetHigh(); }

    IO::FileId GetFile() { return TemplateIdentifier::ident.GetLow(); }

    Identifier<Symlevel::TypeDefinition> GetIdentifier() { return Identifier(GetOffset(), GetFile()); }

    friend class TemplateIdentifier;

protected:
    TypeTemplateIdentifier(PackedIdentifier identifier) : TemplateIdentifier(identifier) {}
};

struct UndefinedTemplateIdentifier : public TemplateIdentifier {
    UndefinedTemplateIdentifier(IndexIdentifier<Term> indexId)
        : TemplateIdentifier(PackedIdentifier(static_cast<uint8_t>(TemplateKind::UNDEFINED), indexId.GetIndex().Raw(), indexId.GetFileId()))
    {
        ASSERT(TemplateKind::UNDEFINED == GetKind());
    }

    IndexIdentifier<Term> GetIndexId()
    {
        auto fileId = TemplateIdentifier::ident.GetLow();
        auto index = Symlevel::Index<Term>(TemplateIdentifier::ident.GetHigh());
        return IndexIdentifier<Term>(index, fileId);
    }

    friend class TemplateIdentifier;

protected:
    UndefinedTemplateIdentifier(PackedIdentifier identifier) : TemplateIdentifier(identifier) {}
};

class Term {
public:
    static constexpr uint16_t FIRST_NON_PRIMITIVE = static_cast<uint16_t>(TemplateKind::UNDEFINED);

    TermData* data;

    static Term Definition(Session& session, Identifier<Symlevel::TypeDefinition> type);

    Term(LocalTerm local);
    Term(GlobalTerm global);

    TemplateIdentifier GetIdentifier() const;
    TemplateKind GetKind() const;
    uint32_t GetLength() const;
    uint32_t Hash() const;

    std::string GetName(Session& session);
    void GetName(Session& session, Stream::Output& stream);

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

    TemplateIdentifier GetIdentifier() const { return Term(*this).GetIdentifier(); }

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

    TemplateIdentifier GetIdentifier() const { return Term(*this).GetIdentifier(); }

    uint32_t GetLength() const { return Term(*this).GetLength(); }

    uint32_t Hash() const { return Term(*this).GetLength(); }

    bool operator==(const GlobalTerm& another) const;
    bool operator!=(const GlobalTerm& another) const;

private:
    friend class Term;
    TermData* data;
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

} // namespace Symlevel
