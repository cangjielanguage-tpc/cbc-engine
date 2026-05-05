#pragma once

#include "engine/engine.h"
#include "engine/identifiers.h"
#include "string.h"
#include "term.h"

namespace Symlevel {

struct MethodReference {
    static MethodReference Parse(Engine::Session& session, Engine::IndexIdentifier<MethodReference> identifier);

    Engine::Identifier<String> name;
    Engine::IndexIdentifier<Term> refType;
    Engine::IndexIdentifier<Term> methodSig;
};

struct FieldReference {
    static FieldReference Parse(Engine::Session& session, Engine::IndexIdentifier<FieldReference> identifier);

    Engine::Identifier<String> name;
    Engine::IndexIdentifier<Term> refType;
    Engine::IndexIdentifier<Term> fieldType;
    bool isRecord;
};

} // namespace Symlevel
