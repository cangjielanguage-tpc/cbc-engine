#pragma once

#include "api/method.h"
#include "engine/symlevel/aot_table.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/references.h"

namespace API {
namespace Impl {

class DirectMethodCbc final : public Method {
public:
    DirectMethodCbc(Engine::Session& session, Symlevel::MethodDefinition def) : session(session), def(def) {}

    Term* ABISignature() override;

    std::optional<Type*> RefType() override;

    std::optional<Interpretation::FunctionHandle*> FUH() override;

    void* TargetAddr() override;

    MethodFlags Flags() override;

    Symlevel::String Name() override;

private:
    Engine::Session& session;
    Symlevel::MethodDefinition def;
};

class DirectMethodAot final : public Method {
public:
    DirectMethodAot(Engine::Session& session, Symlevel::MethodReference ref, Symlevel::DirectCallAotData aotData)
        : session(session),
          ref(ref),
          aotData(aotData)
    {}

    Term* ABISignature() override;

    std::optional<Type*> RefType() override;

    std::optional<Interpretation::FunctionHandle*> FUH() override;

    void* TargetAddr() override;

    MethodFlags Flags() override;

    Symlevel::String Name() override;

private:
    Engine::Session& session;
    Symlevel::MethodReference ref;
    Symlevel::DirectCallAotData aotData;
};

} // namespace Impl
} // namespace API
