#pragma once

#include "engine/identifiers.h"
#include "engine/symlevel/flags.h"
#include "string.h"
#include "term.h"

namespace Symlevel {

struct MethodReference {
    Engine::Identifier<String> name;
    Engine::RefIdentifier<Term> refType;
    Engine::RefIdentifier<Term> methodSig;
    Engine::RefIdentifier<Term> tvars;
    MethodRefFlags flags;
};

struct FieldReference {
    Engine::Identifier<String> name;
    Engine::RefIdentifier<Term> refType;
    Engine::RefIdentifier<Term> fieldType;
    bool isRecord;
};

} // namespace Symlevel
