#pragma once

#include "api/method.h"
#include "engine/symlevel/aot_table.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/references.h"
#include <cstdint>

namespace API {
namespace Impl {

class DirectMethodCbc final : public DirectMethod {
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

class DirectMethodAot final : public DirectMethod {
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

class VirtualMethodImpl final : public VirtualMethod {
public:
    VirtualMethodImpl(Engine::Session& session, Symlevel::MethodReference ref, uint16_t vnum, uint16_t extDefNum)
        : session(session),
          ref(ref),
          vnum(vnum),
          extDefNum(extDefNum)
    {}

    Term* ABISignature() override;

    std::optional<Type*> RefType() override;

    std::optional<Interpretation::FunctionHandle*> FUH() override;

    uint16_t VNum() override;

    uint16_t ExtDefNum() override;

    MethodFlags Flags() override;

    Symlevel::String Name() override;

private:
    Engine::Session& session;
    Symlevel::MethodReference ref;
    uint16_t vnum;
    uint16_t extDefNum;
};

} // namespace Impl
} // namespace API
