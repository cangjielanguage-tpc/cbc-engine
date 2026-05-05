#pragma once

#include "engine/engine.h"
#include "engine/identifiers.h"
#include "string.h"
#include "term.h"

namespace Symlevel {

struct MethodReference {
    static MethodReference Parse(Engine::Session& session, Engine::RefIdentifier<MethodReference> identifier);

    Engine::Identifier<String> name;
    Engine::RefIdentifier<Term> refType;
    Engine::RefIdentifier<Term> methodSig;
};

struct FieldReference {
    static FieldReference Parse(Engine::Session& session, Engine::RefIdentifier<FieldReference> identifier);

    Engine::Identifier<String> name;
    Engine::RefIdentifier<Term> refType;
    Engine::RefIdentifier<Term> fieldType;
    bool isRecord;
};

} // namespace Symlevel
