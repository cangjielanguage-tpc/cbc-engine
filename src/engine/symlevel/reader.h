#pragma once

#include "engine/decode/decoder.h"
#include "engine/engine.h"
#include "engine/symlevel/code.h"
#include "io/file_id.h"
#include "offset.h"

namespace Symlevel {

class Reader {
public:
    template <typename T> static T Read(Engine::Session& session, IO::FileId fileId, Offset<T> offset);
    template <typename T> static T Read(Engine::Session& session, Engine::Identifier<T> id);

    static std::vector<ExceptionRegion> GetExceptionRegions(Engine::Session& session, Code const& code);
    static std::vector<LivenessInfo> GetLivenessInfo(Engine::Session& session, Code const& code);
    static std::vector<StackPtrsInfo> GetStackPtrsInfo(Engine::Session& session, Code const& code);
};

} // namespace Symlevel
