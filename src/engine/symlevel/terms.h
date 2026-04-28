#pragma once

#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/packed_identifier.h"
#include "engine/symlevel/definitions.h"
#include "io/file_id.h"
#include "offset.h"
#include "string.h"
#include "utils/assertion.h"
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

class TemplateIdentifier {
protected:
    constexpr TemplateIdentifier(Engine::PackedIdentifier identifier) : ident(identifier) {}

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

protected:
    Engine::PackedIdentifier ident;
};

struct TagTemplateIdentifier : public TemplateIdentifier {
    constexpr TagTemplateIdentifier(TemplateKind kind)
        : TemplateIdentifier(Engine::PackedIdentifier(static_cast<uint8_t>(kind), 0, 0))
    {
        ASSERT(kind == GetKind());
    }

    friend class TemplateIdentifier;

protected:
    TagTemplateIdentifier(Engine::PackedIdentifier identifier) : TemplateIdentifier(identifier) {}
};

struct AotTypeTemplateIdentifier : public TemplateIdentifier {
    AotTypeTemplateIdentifier(Offset<String> offs, IO::FileId file)
        : TemplateIdentifier(Engine::PackedIdentifier(static_cast<uint8_t>(TemplateKind::AOT_TYPE), offs, file))
    {
        ASSERT(TemplateKind::AOT_TYPE == GetKind());
    }

    Offset<String> GetOffset() { return TemplateIdentifier::ident.GetHigh(); }

    IO::FileId GetFile() { return TemplateIdentifier::ident.GetLow(); }

    friend class TemplateIdentifier;

protected:
    AotTypeTemplateIdentifier(Engine::PackedIdentifier identifier) : TemplateIdentifier(identifier) {}
};

struct TypeTemplateIdentifier : public TemplateIdentifier {
    TypeTemplateIdentifier(Offset<TypeDefinition> offs, IO::FileId file)
        : TemplateIdentifier(Engine::PackedIdentifier(static_cast<uint8_t>(TemplateKind::TYPE), offs, file))
    {
        ASSERT(TemplateKind::TYPE == GetKind());
    }

    TypeTemplateIdentifier(Engine::Identifier<TypeDefinition> type)
        : TypeTemplateIdentifier(type.GetOffset(), type.GetFileId())
    {}

    Offset<TypeDefinition> GetOffset() { return TemplateIdentifier::ident.GetHigh(); }

    IO::FileId GetFile() { return TemplateIdentifier::ident.GetLow(); }

    friend class TemplateIdentifier;

protected:
    TypeTemplateIdentifier(Engine::PackedIdentifier identifier) : TemplateIdentifier(identifier) {}
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
