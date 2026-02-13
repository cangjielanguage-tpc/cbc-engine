#include "engine.h"
#include "loader.h"

#include "symlevel/cbc_file.h"

namespace Engine {

class _Loader {
public:
    _Loader() : fileCounter(0) {}

    int fileCounter;
    std::vector<Symlevel::CbcFile> files;
    std::vector<std::unique_ptr<IO::RandomAccessFile>> rafs;
};

class Engine {
public:
    Engine(std::vector<Symlevel::CbcFile> files,
           std::vector<std::unique_ptr<IO::RandomAccessFile>> rafs)
        : files(std::move(files)), rafs(std::move(rafs)) {}

    static Engine& Instance();

private:
    friend class Session;
    std::vector<Symlevel::CbcFile> files;
    std::vector<std::unique_ptr<IO::RandomAccessFile>> rafs;
};

IO::RandomAccessFile* Session::FileOf(IO::FileId fileId)
{
    // TODO: add session-scoped buffered rafs.
    return engine.rafs.at(fileId).get();
}

Session Session::NewSession(Engine& engine)
{
    return Session(engine);
}

Loader Loader::New() {
    return Loader(std::make_unique<_Loader>());
}

bool Loader::Load(std::unique_ptr<IO::RandomAccessFile> file, std::string_view name)
{
    IO::StreamFileReader reader(*file, 0);
    uint32_t magic = reader.ReadU32();
    if (magic != Symlevel::CbcFile::MAGIC) {
        return false;
    }

    auto id = loader->fileCounter++;
    auto cbcFile = Symlevel::CbcFile::Create(IO::FileId(id), *file, name);
    loader->files.emplace_back(std::move(cbcFile));
    loader->rafs.emplace_back(std::move(file));
    return true;
}

Engine* Loader::Build() {
    return new Engine(
        std::move(loader->files),
        std::move(loader->rafs)
    );
}

} // namespace Engine
