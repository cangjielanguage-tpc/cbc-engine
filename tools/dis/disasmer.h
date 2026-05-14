#pragma once
#include "engine/engine.h"
#include "engine/resolving_output.h"
#include "engine/symlevel/cbc_file.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/string.h"
#include "engine/symlevel/version_metadata.h"
#include "utils/ostream.h"
#include <cstdint>
#include <functional>
#include <memory>

namespace Dis {
//
class Disasmer {
    std::unique_ptr<Engine::Session> SessionFor(std::vector<std::string_view> views);

    void Version(const Symlevel::VersionMetadata& md);

    void Region(std::string name, std::function<void()> fn) { Region(Symlevel::String(name), fn); }

    void Region(Symlevel::String name, std::function<void()> fn);

    void Type(Symlevel::TypeDefinition& def);

    void RData(Symlevel::RegionData const& rd, uint8_t regionNum);

    void SetFile(Symlevel::CbcFile const& file)
    {
        io << "Disassembly of " << file.GetName() << Stream::endl;
        currentFile = &file;
    }

    Symlevel::CbcFile const* currentFile = nullptr;
    std::unique_ptr<Engine::Session> session;
    std::vector<Symlevel::CbcFile> const& files;
    Stream::Indented idio;
    Stream::ResolvingOutput io = Stream::ResolvingOutput(*session, idio);

    bool resolving;

    void DisasmOf(Symlevel::CbcFile const& file);

public:
    void Disasm()
    {
        for (auto& file : files) {
            DisasmOf(file);
        }
    }

    Disasmer(std::vector<std::string_view> views, Stream::Output& s, bool resolving)
        : session(SessionFor(views)),
          files(session->GetEngine().Files()),
          resolving(resolving),
          idio(s, 0)
    {}
};
} // namespace Dis
