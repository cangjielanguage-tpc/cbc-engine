#pragma once
#include "engine/engine.h"
#include "engine/image/cbc_file.h"
#include "engine/image/version_metadata.h"
#include "engine/resolving_output.h"
#include "utils/function.h"
#include "utils/ostream.h"
#include "utils/span.h"
#include <cstdint>
#include <memory>

namespace Dis {
//
class Disasmer {
    Engine::Session SessionFor(Utils::Vector<std::string_view> views);

    void Version(const Image::VersionMetadata& md);

    void Region(std::string name, Utils::Function<void()> fn) { Region(Image::String(name), fn); }

    void Region(Image::String name, Utils::Function<void()> fn);

    void Type(Image::TypeDefinition& def);

    void RData(Image::RegionData const& rd, uint8_t regionNum);

    void SetFile(Image::CbcFile const& file) { currentFile = &file; }

    Image::CbcFile const* currentFile = nullptr;
    Engine::Session session;
    Utils::Span<Image::CbcFile const> files;
    Stream::Indented idio;
    Stream::ResolvingOutput io = Stream::ResolvingOutput(session, idio);

    bool resolving;

    void DisasmOf(Image::CbcFile const& file);

public:
    void Disasm()
    {
        for (auto& file : files) {
            DisasmOf(file);
        }
    }

    Disasmer(Utils::Vector<std::string_view> views, Stream::Output& s, bool resolving)
        : session(SessionFor(views)),
          files(session.GetEngine().Files()),
          idio(s, 0),
          resolving(resolving)
    {}
};
} // namespace Dis
