#pragma once

#include "cbc/abi.h"
#include "cbc/c_abi.h"
#include "cbc/isa.h"
#include "cbc/move_resolver.h"
#include "engine/terms.h"
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
    CbcTypeKind kind = CbcTypeKind::INVALID;
    Engine::Term term {}; // used only for by-value record arguments of foreign calls
    bool isVarArgLabel = false;
    bool isVArray      = false;
};

struct CallSignature {
    Utils::Vector<ParamInfo> params;
    Engine::Term retType {};
    bool isVarArg    = false;
    int funcVarCount = 0;
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
    int stackSlots = 0;
    int fregs      = 0;
    StagingResult result;
};

class CallStager {
public:
    CallStager(Resolution::Resolver* resolver, MoveEmitter& moveEmitter) : resolver(resolver), moveEmitter(moveEmitter)
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
        const CallSignature& sig,
        std::optional<CAbiStructPlan> retPlan
    );

    std::optional<CAbiStructPlan> StageForeignStructRet(const CallSignature& sig, const CallAbiFlags& flags);

private:
    struct ForeignStructCopy {
        Location srcAddr; // location of caller-owned struct
        Location ptrLoc;  // where the pointer to the callee-owned copy is saved
        uint32_t size;
        uint32_t dstOffset = 0; // byte offset of the copy from the frame bottom
    };

    bool CheckArgCount(size_t argCount, const CallAbiFlags& flags, const CallSignature& sig, StagingResult& result);

    bool StageForeignStructArg(
        Location srcAddr,
        const ParamInfo& param,
        bool varArgPos,
        MoveResolver& moves,
        AbiAssigner& abi,
        Utils::Vector<ForeignStructCopy>& copies
    );

    void Fail(StagingResult& result, std::string&& msg);

    Resolution::Resolver* resolver;
    MoveEmitter& moveEmitter;
};

CallSignature MakeCallSignature(const Resolution::MethodSignature& sig);

} // namespace Cbc
