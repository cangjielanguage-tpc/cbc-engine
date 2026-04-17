#pragma once

#include "api/method.h"
#include "engine/symlevel/aot_table.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/references.h"
#include "resolver_impl.h"
#include <cstdint>

namespace API {
namespace Impl {

class DirectMethodCbc final : public DirectMethod {
public:
    DirectMethodCbc(Type* refType, Interpretation::FunctionHandle* fuh, Symlevel::String name)
        : refType(refType),
          fuh(fuh),
          name(name)
    {}

    Symlevel::Term* ABISignature() const override
    {
        FATAL("not implemented yet");
        return nullptr;
    };

    Type* RefType() const override { return refType; }

    Interpretation::FunctionHandle* FUH() const override { return fuh; }

    AotCodeAddr TargetAddr() const override { return nullptr; }

    MethodFlags Flags() const override
    {
        FATAL("not implemented yet");
        return MethodFlags();
    }

    Symlevel::String Name() const override { return name; }

private:
    Type* refType;
    Interpretation::FunctionHandle* fuh;
    Symlevel::String name;
};

class DirectMethodAot final : public DirectMethod {
public:
    DirectMethodAot(Type* refType, AotCodeAddr targetAddress, Symlevel::String name)
        : refType(refType),
          targetAddress(targetAddress),
          name(name)
    {}

    Symlevel::Term* ABISignature() const override
    {
        FATAL("not implemented yet");
        return nullptr;
    }

    Type* RefType() const override { return refType; }

    Interpretation::FunctionHandle* FUH() const override { return nullptr; }

    AotCodeAddr TargetAddr() const override { return targetAddress; }

    MethodFlags Flags() const override
    {
        FATAL("not implemented yet");
        return MethodFlags();
    }

    Symlevel::String Name() const override { return name; }

private:
    Type* refType;
    AotCodeAddr targetAddress;
    Symlevel::String name;
};

class VirtualMethodImpl final : public VirtualMethod {
public:
    VirtualMethodImpl(Type* refType, uint16_t vnum, uint16_t extDefNum, Symlevel::String name)
        : refType(refType),
          vnum(vnum),
          extDefNum(extDefNum),
          name(name)
    {}

    Symlevel::Term* ABISignature() const override
    {
        FATAL("not implemented yet");
        return nullptr;
    }

    Type* RefType() const override { return refType; }

    uint16_t VNum() const override { return vnum; }

    uint16_t ExtDefNum() const override { return extDefNum; }

    MethodFlags Flags() const override
    {
        FATAL("not implemented yet");
        return MethodFlags();
    }

    Symlevel::String Name() const override { return name; }

private:
    Type* refType;
    Symlevel::String name;
    uint16_t vnum;
    uint16_t extDefNum;
};

class InterfaceMethodImpl final : public InterfaceMethod {
public:
    InterfaceMethodImpl(Type* refType, uint16_t inum, Symlevel::String name) : refType(refType), inum(inum), name(name)
    {}

    Symlevel::Term* ABISignature() const override
    {
        FATAL("not implemented yet");
        return nullptr;
    }

    Type* RefType() const override { return refType; }

    uint16_t INum() const override { return inum; }

    MethodFlags Flags() const override
    {
        FATAL("not implemented yet");
        return MethodFlags();
    }

    Symlevel::String Name() const override { return name; }

private:
    Type* refType;
    Symlevel::String name;
    uint16_t inum;
};

} // namespace Impl
} // namespace API
