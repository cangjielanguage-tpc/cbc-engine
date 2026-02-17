#include "engine.h"
#include "loader.h"

#include "symlevel/cbc_file.h"

namespace Engine {

class Engine::Impl {
public:
    Impl(std::vector<Symlevel::CbcFile> files,
           std::vector<std::unique_ptr<IO::RandomAccessFile>> rafs)
        : files(std::move(files)), rafs(std::move(rafs)) {}

    static Engine& Instance();

private:
    friend class Session;
    std::vector<Symlevel::CbcFile> files;
    std::vector<std::unique_ptr<IO::RandomAccessFile>> rafs;
};

class Loader::Impl {
public:
    Impl() : fileCounter(0) {}

    int fileCounter;
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

Session Session::NewSession(Engine& engine)
{
    return Session(engine);
}

Arena& Session::Allocator()
{
    return arena;
}

Loader Loader::New() {
    return Loader(new Impl());
}

Loader::~Loader() {
    delete loader;
}

bool Loader::Load(std::unique_ptr<IO::RandomAccessFile> file, std::string_view name)
{
    IO::StreamFileReader reader(*file, 0);
    uint32_t magic = reader.ReadU32();
    if (magic != Symlevel::CbcFile::MAGIC) {
        return false;
    }

    auto id = loader->fileCounter++;
    //auto cbcFile = Symlevel::CbcFile::Create(IO::FileId(id), *file, name);
    loader->files.emplace_back(std::move(Symlevel::CbcFile::Create(IO::FileId(id), *file, name)));
    loader->rafs.emplace_back(std::move(file));
    return true;
}

Engine Loader::Build() {
    return Engine(new Engine::Impl(
        std::move(loader->files),
        std::move(loader->rafs)
    ));
}

Engine::~Engine() {
    delete impl;
}

} // namespace Engine
