#include "exception_handling.h"

#include <algorithm>

#include "engine/symlevel/code.h"
#include "engine/symlevel/definitions.h"

uint8_t engine_get_exception_handler(Interpretation::DynamicFunctionHandle* handle, Decoder::ByteReader& reader)
{
    Engine::Session session(Engine::GetEngineInstance());
    auto bytecode     = handle->bytecode.load();
    auto offsetsIndex = bytecode->offsetsIndex;

    auto methodDef = Symlevel::MethodDefinition::Resolve(session, handle->methodDef);
    if (!methodDef.MethodCode().has_value()) {
        return false;
    }

    uint8_t* bcStart = bytecode->code.bytecode;
    uint8_t* bcEnd   = bcStart + bytecode->code.bytecodeSize;
    ASSERT(bcStart <= reader.Cursor() && reader.Cursor() <= bcEnd);

    auto bcPos      = reader.Cursor() - bcStart;
    auto exPos      = bcPos - 1;
    auto methodCode = Symlevel::Code::Resolve(session, methodDef.MethodCode().value());
    auto regions    = methodCode.GetExceptionRegions(session);
    auto it         = std::find_if(regions.begin(), regions.end(), [&](const Symlevel::ExceptionRegion& region) {
        auto regStart = offsetsIndex.FindMappedOffset(Cbc::InstructionType::CBC, region.start);
        auto regEnd   = offsetsIndex.FindMappedOffset(Cbc::InstructionType::CBC, region.end);

        if (!regStart.has_value() || !regEnd.has_value()) {
            FATAL("Couldn't translate exception region offsets to rt bytecode offsets");
            return false;
        }

        return regStart.value() <= exPos && exPos <= regEnd.value();
    });

    if (it == regions.end()) {
        return false; // no suitable handler found
    }

    auto target = offsetsIndex.FindMappedOffset(Cbc::InstructionType::CBC, it->target);
    if (!target.has_value()) {
        FATAL("Couldn't translate exception region target offset to rt bytecode offset");
        return false;
    }

    auto delta = static_cast<int64_t>(target.value()) - static_cast<int64_t>(bcPos);
    reader.Advance(delta);
    return true;
}
