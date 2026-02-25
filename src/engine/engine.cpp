#include "engine.h"
#include "interpreter/function_handle.h"
#include "symlevel/cbc_file.h"
#include "symlevel/definitions.h"
#include "symlevel/io/stream_file_reader.h"
#include "symlevel/reader.h"

namespace Engine {

using namespace Symlevel;

class Engine::Impl {
public:
    Impl(std::vector<CbcFile> files, std::vector<std::unique_ptr<IO::RandomAccessFile>> rafs)
        : files(std::move(files)),
          rafs(std::move(rafs)),
          fuhManager()
    {}

    static Engine::Impl& Of(Engine& engine) { return *engine.impl; }

    friend class Session;
    std::vector<CbcFile> files;
    std::vector<std::unique_ptr<IO::RandomAccessFile>> rafs;

    Interpretation::FunctionHandleManager fuhManager;
    DefinitionsManager defsManager;
};

class Loader::Impl {
public:
    Impl() : fileCounter(0) {}

    uint32_t fileCounter;
    std::vector<CbcFile> files;
    std::vector<std::unique_ptr<IO::RandomAccessFile>> rafs;
};

/////////////////////////////////////////////////////////////////

std::unique_ptr<IO::RandomAccessFile>& Session::FileOf(IO::FileId fileId) const
{
    // TODO: add session-scoped buffered rafs.
    return engine.impl->rafs.at(fileId);
}

CbcFile& Session::CbcFileOf(IO::FileId fileId) const
{
    // TODO: add session-scoped buffered rafs.
    return engine.impl->files.at(fileId);
}

Arena& Session::Allocator() { return arena; }

Loader::Loader() : loader(std::move(std::make_unique<Loader::Impl>())) {}

Loader::Loader(Loader&& other) = default;
Loader::~Loader()              = default;

std::pmr::memory_resource& Engine::CodeHeap() const { return *std::pmr::new_delete_resource(); }

bool Loader::Load(std::unique_ptr<IO::RandomAccessFile> file, std::string_view fileName)
{
    IO::StreamFileReader reader(*file, 0);
    uint32_t magic = reader.ReadU32();
    if (magic != CbcFile::MAGIC) {
        return false;
    }

    auto id = loader->fileCounter++;
    loader->files.emplace_back(std::move(CbcFile::Create(IO::FileId(id), *file, fileName)));
    loader->rafs.emplace_back(std::move(file));
    return true;
}

Engine Loader::Build()
{
    return Engine(std::move(std::make_unique<Engine::Impl>(std::move(loader->files), std::move(loader->rafs))));
}

Engine::Engine(std::unique_ptr<Engine::Impl>&& impl) : impl(std::move(impl)) {}

Engine::Engine(Engine&& other) = default;
Engine::~Engine()              = default;

std::optional<Identifier<TypeDefinition>> Engine::FindType(Session& session, IO::FileId fileId, std::string_view name)
{
    auto& file     = session.CbcFileOf(fileId);
    auto& raf      = session.FileOf(fileId);
    auto tableOffs = file.GetTypesTableOffs();
    IO::StreamFileReader reader(*raf, tableOffs);

    uint32_t entryCount = reader.ReadU32();
    for (uint32_t i = 0; i < entryCount; i++) {
        auto typeOffs = Offset<TypeDefinition>(reader.ReadU32());
        auto typeDef  = TypeDefinition::Parse(session, fileId, typeOffs);
        auto typeName = Reader::Read(session, fileId, typeDef.Name());
        if (typeName.compare(name) == 0) {
            return typeDef.GetIdentifier();
        }
    }
    return std::nullopt;
}

std::optional<Identifier<MethodDefinition>> Engine::FindMain(Session& session, std::string_view fileName)
{
    // FIXME: search for proper enclosing type and method name
    for (auto& file : impl->files) {
        if (file.GetName() != fileName) {
            continue;
        }
        auto id          = file.Id();
        auto& raf        = session.FileOf(id);
        auto defaultType = FindType(session, id, "default");
        if (!defaultType.has_value()) {
            continue;
        }
        auto typeDef = TypeDefinition::Resolve(session, defaultType.value());
        IO::StreamFileReader reader(*raf, typeDef.GetMethodsTableOffs());
        auto methodCount = typeDef.GetMethodCount();
        for (uint32_t i = 0; i < methodCount; i++) {
            auto offset     = Offset<MethodDefinition>(reader.ReadU32());
            auto def        = MethodDefinition::Parse(session, id, offset);
            auto methodName = Reader::Read(session, id, def.Name());
            if (methodName.compare("main") == 0) {
                return def.GetIdentifier();
            }
        }
    }
    return std::nullopt;
}

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
} // namespace Symlevel
