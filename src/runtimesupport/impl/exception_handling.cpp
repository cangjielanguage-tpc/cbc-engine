#include "exception_handling.h"

#include <algorithm>

#include "engine/symlevel/code.h"
#include "engine/symlevel/definitions.h"

uint8_t engine_get_exception_handler(Interpretation::DynamicFunctionHandle* handle, Decoder::FatByteReader& reader)
{
    Engine::Session session(Engine::GetEngineInstance());
    auto offsetsIndex = handle->bytecode.load()->offsetsIndex;

    auto methodDef = Symlevel::MethodDefinition::Resolve(session, handle->methodDef);
    if (!methodDef.MethodCode().has_value()) {
        return false;
    }

    auto methodCode = Symlevel::Code::Resolve(session, methodDef.MethodCode().value());
    auto regions    = methodCode.GetExceptionRegions(session);
    auto it         = std::find_if(regions.begin(), regions.end(), [&](const Symlevel::ExceptionRegion& region) {
        auto start = offsetsIndex.FindMappedOffset(Cbc::InstructionType::CBC, region.start);
        auto end   = offsetsIndex.FindMappedOffset(Cbc::InstructionType::CBC, region.end);

        if (!start.has_value() || !end.has_value()) {
            FATAL("Couldn't translate exception region offsets to rt bytecode offsets");
            return false;
        }

        auto pos = reader.Pos() - 1;
        return start.value() <= pos && pos <= end.value();
    });

    if (it == regions.end()) {
        return false; // no suitable handler found
    }

    auto target = offsetsIndex.FindMappedOffset(Cbc::InstructionType::CBC, it->target);
    if (!target.has_value()) {
        FATAL("Couldn't translate exception region target offset to rt bytecode offset");
        return false;
    }

    auto delta = static_cast<int64_t>(target.value()) - static_cast<int64_t>(reader.Pos());
    reader.Advance(delta);
    return true;
}
