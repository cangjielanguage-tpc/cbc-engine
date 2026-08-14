#pragma once

#include "engine/identifiers.h"
#include "engine/symlevel/code.h"
#include "engine/symlevel/flags.h"
#include "engine/symlevel/sequence.h"
#include "io/file_id.h"
#include "member_index.h"
#include "offset.h"
#include "string.h"
#include "term.h"
#include <cstdint>
#include <optional>

namespace Symlevel {

enum class EnumKind : uint8_t {
    NOT_ENUM,
    UNION,
    OPTION0, // enum { Some(T); None }
    OPTION1, // enum { None; Some(T) }
    PRIMITIVE,
};

class TypeDefinition {
public:
    struct Content {
        Engine::Identifier<TypeDefinition> identifier;
        Engine::Identifier<String> name;
        MethodIndex methods;
        FieldIndex fields;
        OffsetSequence<MethodDefinition> virtualMethods;
        OffsetSequence<FieldDefinition> instanceFields;
        Engine::RefIdentifier<Term> superOrEnumType;
        TypeFlags flags;
        uint8_t arity;
        EnumKind enumKind;
        RefSequence<Term> interfaces {};
        RefSequence<Term> unionFields {};
    };

    Engine::Identifier<TypeDefinition> const GetIdentifier() { return content.identifier; }

    Engine::Identifier<String> const GetName() const { return content.name; }

    MethodIndex const GetMethods() const { return content.methods; }

    FieldIndex const GetFields() const { return content.fields; }

    OffsetSequence<MethodDefinition> const GetVirtualMethods() const { return content.virtualMethods; }

    OffsetSequence<FieldDefinition> const GetInstanceFields() const { return content.instanceFields; }

    Engine::RefIdentifier<Term> const GetSuperType() const {
        if (content.enumKind != EnumKind::NOT_ENUM) {
            return Engine::RefIdentifier(RefId<Term>(0), IO::FileId(0)); // NIL TERM
        }
        return content.superOrEnumType;
    }

    Engine::RefIdentifier<Term> const GetEnumType() const
    {
        if (content.enumKind == EnumKind::NOT_ENUM) {
            return Engine::RefIdentifier(RefId<Term>(0), IO::FileId(0)); // NIL TERM
        }
        return content.superOrEnumType;
    }

    TypeFlags const GetFlags() const { return content.flags; }

    RefSequence<Term> GetInterfaces() const { return content.interfaces; }

    Content const* operator->() const { return &content; }

    Content const* operator*() const { return &content; }

    TypeDefinition(Content&& content) : content(content) {}

    Content content;
};

class FieldDefinition {
public:
    struct Content {
        Engine::Identifier<FieldDefinition> identifier;
        Offset<String> nameOffset;
        Engine::RefIdentifier<Term> fieldType;
        FieldFlags flags;
    };

    inline Engine::Identifier<String> GetName() const { return Engine::Identifier(content.nameOffset, content.identifier.GetFileId()); }

    inline Engine::RefIdentifier<Term> FieldType() const { return content.fieldType; }

    inline Engine::Identifier<FieldDefinition> Identifier() { return content.identifier; }

    inline FieldFlags Flags() const { return content.flags; }

    FieldDefinition(Content&& content) : content(content) {}

    Content content;
};

class MethodDefinition {
public:
    struct Content {
        Engine::Identifier<MethodDefinition> identifier;
        Engine::RefIdentifier<Term> signature;
        Offset<String> typeNameOffset;
        Offset<String> nameOffset;
        MethodFlags flags;
        uint8_t arity;

        std::optional<Engine::Identifier<Code>> code             = std::nullopt;
        std::optional<Engine::Identifier<String>> sourceFile     = std::nullopt;
        std::optional<Engine::Identifier<String>> sourceFullName = std::nullopt;
        std::optional<Engine::Identifier<String>> linkageName    = std::nullopt;
    };

    inline Engine::Identifier<String> Name() const { return Engine::Identifier(content.nameOffset, content.identifier.GetFileId()); }

    inline Engine::Identifier<String> TypeName() const
    {
        return Engine::Identifier(content.typeNameOffset, content.identifier.GetFileId());
    }

    inline Engine::RefIdentifier<Term> Signature() const { return content.signature; }

    inline std::optional<Engine::Identifier<Code>> MethodCode() const { return content.code; }

    std::optional<Engine::Identifier<String>> SourceFile() const { return content.sourceFile; }

    std::optional<Engine::Identifier<String>> SourceFullName() const { return content.sourceFullName; }

    std::optional<Engine::Identifier<String>> LinkageName() const { return content.linkageName; }

    inline IO::FileId FileId() const { return content.identifier.GetFileId(); }

    Engine::Identifier<MethodDefinition> GetIdentifier() const { return content.identifier; }

    MethodFlags GetFlags() const { return content.flags; }

    MethodRefFlags GetABIFlags() const;

    Content const* operator->() const { return &content; }

    MethodDefinition(Content&& content) : content(content) {}

    Content content;
};

} // namespace Symlevel
