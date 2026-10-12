#include "cbc_engine.h"
#include "engine/engine.h"
#include "engine/image/cbc_file.h"
#include "engine/resolving_output.h"
#include "utils/ostream.h"

#include <cstdint>

CBC_EXPORT extern "C" char* engine_dbg_mdef_name(uint64_t methodDef)
{
    Engine::Session session(Engine::GetEngineInstance());
    Image::Identifier<Image::MethodDefinition> mdef(methodDef);
    Stream::StringBuffer buf;
    Stream::ResolvingOutput out(session, buf);
    out << Stream::Detailed(mdef);
    // Intentionally leak the memory.
    return buf.ToCString();
}
