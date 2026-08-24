#pragma once

#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/symlevel/flags.h"
#include "string.h"
#include "term.h"

#include <vector>

namespace Symlevel {

struct MethodReference {
    static MethodReference Parse(Engine::Session& session, Engine::RefIdentifier<MethodReference> identifier);

    Engine::Identifier<String> name;
    Engine::RefIdentifier<Term> refType;
    Engine::RefIdentifier<Term> methodSig;
    Engine::RefIdentifier<Term> tvars;
    MethodRefFlags flags;
};

enum FieldRefTag {
    SINGLE,
    CONST_INDEX,
    MULTI,
    NONE
};

struct FieldReference {
    FieldRefTag tag;

    union {
        struct {
            Engine::Identifier<String> name;
            Engine::RefIdentifier<Term> refType;
            Engine::RefIdentifier<Term> fieldType;
        } single;

        struct {
            uint32_t idx;
            Engine::RefIdentifier<Term> refType;
            Engine::RefIdentifier<Term> fieldType;
        } constIndex;

        struct {
            uint32_t length;
            FieldReference* subRefs;
            RefId<FieldReference>* indices;
        } multi;

        struct {
            Engine::RefIdentifier<Term> sig;
        } none;
    };

    FieldReference(
        Engine::Identifier<String> name, Engine::RefIdentifier<Term> refType, Engine::RefIdentifier<Term> fieldType
    )
        : tag(SINGLE),
          single({ name, refType, fieldType })
    {}

    FieldReference(uint32_t idx, Engine::RefIdentifier<Term> refType, Engine::RefIdentifier<Term> fieldType)
        : tag(CONST_INDEX),
          constIndex({ idx, refType, fieldType })
    {}

    FieldReference(uint32_t length, FieldReference* subRefs, RefId<FieldReference>* indices)
        : tag(MULTI),
          multi({ length, subRefs, indices })
    {}

    FieldReference(Engine::RefIdentifier<Term> sig) : tag(NONE), none({ sig }) {}

    static FieldReference Parse(Engine::Session& session, Engine::RefIdentifier<FieldReference> identifier);

    const char* KindAsString()
    {
        switch (tag) {
            case SINGLE:      return "Single";
            case CONST_INDEX: return "ConstIndex";
            case MULTI:       return "Multi";
            case NONE:        return "NONE";
        }
    }
};

} // namespace Symlevel
