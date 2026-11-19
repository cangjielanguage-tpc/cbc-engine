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

namespace Resolution {
struct Resolver;
struct MethodSignature;
} // namespace Resolution

namespace Cbc {

struct ParamInfo {
    CbcTypeKind kind   = CbcTypeKind::INVALID;
    bool isVarArgLabel = false;
    bool isVArray      = false;
    Utils::Vector<CAbiField> fields; // filled only for foreign by-value records
    uint32_t flatSize = 0;
};

/// Managed (Cangjie ABI) call: no varargs, no by-value records.
struct CallSignature {
    Utils::Vector<ParamInfo> params;
    int funcVarCount = 0;
};

/// Foreign (C ABI) call: record layouts are resolved by the time it is built.
struct ForeignCallSignature {
    Utils::Vector<ParamInfo> params;
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

struct ForeignStaging {
    int stackBytes = 0;
    int fregs      = 0;
    StagingResult result;
};

class CallStager {
public:
    /// `desc` selects the ABI; defaults to the host platform.
    CallStager(MoveEmitter& moveEmitter, const AbiAssigner::PlatformDescription& desc, CAbi cAbi)
        : moveEmitter(moveEmitter),
          desc(desc),
          cAbi(cAbi)
    {}

    StagingResult StageManaged(
        Utils::Span<const Location> args,
        const CallAbiFlags& flags,
        const CallSignature& sig,
        Location* outerTiLoc = nullptr,
        Location* thisTiLoc  = nullptr,
        bool genericParams   = false
    );

    ForeignStaging StageForeign(
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

    bool CheckManagedArgCount(
        size_t argCount, const CallAbiFlags& flags, const CallSignature& sig, StagingResult& result
    );

    bool CheckForeignArgCount(
        size_t argCount, const CallAbiFlags& flags, const ForeignCallSignature& sig, StagingResult& result
    );

    bool StageForeignStructArg(
        Location srcAddr,
        const ParamInfo& param,
        bool varArgPos,
        MoveResolver& moves,
        AbiAssigner& abi,
        Utils::Vector<ForeignStructCopy>& copies
    );

    static StagingResult Fail(std::string&& msg);

    MoveEmitter& moveEmitter;
    AbiAssigner::PlatformDescription desc;
    CAbi cAbi;
};

CallSignature MakeCallSignature(const Resolution::MethodSignature& sig);

std::optional<ForeignCallSignature> MakeForeignCallSignature(
    Resolution::Resolver& resolver, const Resolution::MethodSignature& sig
);

} // namespace Cbc
