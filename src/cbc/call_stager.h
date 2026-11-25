#pragma once

#include "cbc/abi.h"
#include "cbc/c_abi.h"
#include "cbc/isa.h"
#include "cbc/move_resolver.h"
#include "engine/image/flags.h"
#include "utils/span.h"
#include "utils/vector.h"

#include <optional>
#include <string>

namespace Engine {
struct Term;
} // namespace Engine

namespace Resolution {
struct Resolver;
struct MethodSignature;
} // namespace Resolution

namespace Cbc {

struct ManagedArg {
    enum class Role {
        Sret,
        Derived,
        Receiver,
        Param,
        FuncVar,
        OuterTi,
        ThisTi,
    };
    CbcTypeKind kind;
    Role role;
};

/// Stack-passed parameter as seen by the callee: its byte offset from the
/// tail base (the `n`-th stack parameter is `ld.tail` operand `n`) and the
/// load kind that reads exactly what the caller-side store wrote.
struct TailParamInfo {
    uint32_t offset;
    Format::LoadAccessKind ldk;
};

struct ForeignParam {
    ForeignParam() = default;

    ForeignParam(CbcTypeKind kind) : kind(kind) {}

    CbcTypeKind kind   = CbcTypeKind::INVALID;
    bool isVarArgLabel = false;
    bool isVArray      = false;
    Utils::Vector<CAbiField> fields; // filled only for by-value records
    uint32_t flatSize = 0;
};

/// Managed (Cangjie ABI) call: no varargs, records are always passed as 8-byte pointers.
struct ManagedCallSignature {
    Utils::Vector<CbcTypeKind> params;
    int funcVarCount = 0;
};

struct ForeignCallSignature {
    Utils::Vector<ForeignParam> params;
    bool isVarArg = false;
    Utils::Vector<CAbiField> retFields;
    uint32_t retSize = 0;
};

struct CallAbiFlags {
    bool isSRet;
    bool hasOuterTi;
    Image::MethodAbiKind abiKind;
};

struct StagingResult {
    Utils::Vector<uint32_t> paramRefSlots;
    Utils::Vector<uint32_t> paramRecSlots;
    std::optional<int> derived;
    uint32_t maxParamPassingSize = 0;
    bool ok                      = true;
    std::string failure;
};

struct ForeignStagingResult {
    int stackBytes = 0;
    int fregs      = 0;
    StagingResult result;
};

class CallStager {
public:
    CallStager(MoveEmitter& moveEmitter, const AbiAssigner::PlatformDescription& desc)
        : moveEmitter(moveEmitter),
          desc(desc)
    {}

    StagingResult StageManaged(
        Utils::Span<const Location> args,
        const CallAbiFlags& flags,
        const ManagedCallSignature& sig,
        Location* outerTiLoc = nullptr,
        Location* thisTiLoc  = nullptr,
        bool genericParams   = false
    );

    ForeignStagingResult StageForeign(
        Utils::Span<const Location> args,
        const CallAbiFlags& flags,
        const ForeignCallSignature& sig,
        std::optional<CAbiStructPlan> retPlan
    );

    std::optional<CAbiStructPlan> StageForeignStructRet(const ForeignCallSignature& sig, const CallAbiFlags& flags);

private:
    struct ForeignStructCopy {
        Location srcAddr; // location of caller-owned struct
        ArgTarget ptrLoc; // where the pointer to the callee-owned copy is saved
        uint32_t size;
        uint32_t dstOffset = 0; // byte offset of the copy from the frame bottom
    };

    bool CheckForeignArgCount(
        size_t argCount, const CallAbiFlags& flags, const ForeignCallSignature& sig, StagingResult& result
    );

    void StageForeignStructArg(
        Location srcAddr,
        const ForeignParam& param,
        bool varArgPos,
        MoveResolver& moves,
        AbiAssigner& abi,
        Utils::Vector<ForeignStructCopy>& copies
    );

    static StagingResult Fail(std::string&& msg);

    MoveEmitter& moveEmitter;
    AbiAssigner::PlatformDescription desc;
};

/// Arguments of a managed call in ABI order: sret, receiver, parameters,
/// function variables, outer/this type infos. Shared by call staging and the
/// callee-side layouts so their orderings agree by construction.
Utils::Vector<ManagedArg> ManagedArgSequence(
    const ManagedCallSignature& sig, const CallAbiFlags& flags, bool genericParams
);

ManagedCallSignature MakeManagedCallSignature(const Resolution::MethodSignature& sig);

ManagedCallSignature MakeManagedCallSignature(Engine::Term signature);

std::optional<ForeignCallSignature> MakeForeignCallSignature(
    Resolution::Resolver& resolver, const Resolution::MethodSignature& sig
);

/// Tail layout of a managed method: one entry per stack-passed parameter, in
/// `ld.tail` ordinal order. Mirrors the placement `StageManaged` produces for
/// a call to the same signature.
Utils::Vector<TailParamInfo> ComputeTailParams(
    const ManagedCallSignature& sig, const CallAbiFlags& flags, const AbiAssigner::PlatformDescription& desc
);

} // namespace Cbc
