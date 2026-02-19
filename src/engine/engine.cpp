#include "engine.h"
#include "symlevel/cbc_file.h"
#include "symlevel/io/stream_file_reader.h"
#include "interpreter/function_handle.h"
#include "symlevel/definitions.h"

namespace Engine {

class Engine::Impl {
public:
    Impl(std::vector<Symlevel::CbcFile> files, std::vector<std::unique_ptr<IO::RandomAccessFile>> rafs) :
        files(std::move(files)),
        rafs(std::move(rafs)),
        fuhManager()
    {}

    static Engine::Impl& Of(Engine& engine) { return *engine.impl; }

    friend class Session;
    std::vector<Symlevel::CbcFile> files;
    std::vector<std::unique_ptr<IO::RandomAccessFile>> rafs;

    Interpretation::FunctionHandleManager fuhManager;
    Symlevel::DefinitionsManager defsManager;
};

class Loader::Impl {
public:
    Impl() : fileCounter(0) {}

    uint32_t fileCounter;
    std::vector<Symlevel::CbcFile> files;
    std::vector<std::unique_ptr<IO::RandomAccessFile>> rafs;
};

/////////////////////////////////////////////////////////////////

std::unique_ptr<IO::RandomAccessFile>& Session::FileOf(IO::FileId fileId) const
{
    // TODO: add session-scoped buffered rafs.
    return engine.impl->rafs.at(fileId);
}

Symlevel::CbcFile& Session::CbcFileOf(IO::FileId fileId) const
{
    // TODO: add session-scoped buffered rafs.
    return engine.impl->files.at(fileId);
}

Arena& Session::Allocator() { return arena; }

Loader::Loader() : loader(std::move(std::make_unique<Loader::Impl>())) {}
Loader::Loader(Loader&& other) = default;
Loader::~Loader() = default;

std::pmr::memory_resource& Engine::CodeHeap() const
{
    return *std::pmr::new_delete_resource();
}

bool Loader::Load(std::unique_ptr<IO::RandomAccessFile> file, std::string_view name)
{
    IO::StreamFileReader reader(*file, 0);
    uint32_t magic = reader.ReadU32();
    if (magic != Symlevel::CbcFile::MAGIC) {
        return false;
    }

    auto id = loader->fileCounter++;
    loader->files.emplace_back(std::move(Symlevel::CbcFile::Create(IO::FileId(id), *file, name)));
    loader->rafs.emplace_back(std::move(file));
    return true;
}

Engine Loader::Build() {
    return Engine(std::move(std::make_unique<Engine::Impl>(
        std::move(loader->files),
        std::move(loader->rafs)
    )));
}

Engine::Engine(std::unique_ptr<Engine::Impl>&& impl) : impl(std::move(impl)) {}
Engine::Engine(Engine&& other) = default;
Engine::~Engine() = default;

} // namespace Engine

namespace Interpretation {

FunctionHandleManager& FunctionHandleManager::Of(Engine::Engine& engine)
{
    return Engine::Engine::Impl::Of(engine).fuhManager;
}

} // namespace Interpretation

namespace Symlevel {

DefinitionsManager& DefinitionsManager::Of(Engine::Engine& engine)
{
    return Engine::Engine::Impl::Of(engine).defsManager;
}
}
