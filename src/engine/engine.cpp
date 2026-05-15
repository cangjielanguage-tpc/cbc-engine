#include "engine.h"
#include "engine/method_table.h"
#include "engine/statics_manager.h"
#include "engine/terms.h"
#include "engine/typeinfo_manager.h"
#include "interpreter/function_handle.h"
#include "symlevel/cbc_file.h"
#include "symlevel/definitions.h"
#include "symlevel/io/stream_file_reader.h"
#include "symlevel/member_index.h"
#include "utils/heap.h"
#include <memory>

namespace Engine {

using namespace Symlevel;

static Engine* g_engineInstance;

/////////////////////////////////////////////////////////////////
// Impl definitions

class Engine::Impl {
public:
    Impl(std::vector<CbcFile> files, std::vector<std::unique_ptr<IO::RandomAccessFile>> rafs)
        : files(std::move(files)),
          rafs(std::move(rafs)),
          typeInfoManager(TypeInfoManager::NewInstance()),
          mtManager(MethodTableManager::NewInstance())
    {}

    static Engine::Impl& Of(Engine& engine) { return *engine.impl; }

    std::optional<CbcFile*> FindCbcFile(std::string_view filePath);

    friend class Session;
    std::vector<CbcFile> files;
    std::vector<std::unique_ptr<IO::RandomAccessFile>> rafs;

    Interpretation::FunctionHandleManager fuhManager;
    DefinitionsManager defsManager;
    std::unique_ptr<MethodTableManager> mtManager;
    TermManager termManager;
    StaticsManager staticsManager;
    std::unique_ptr<TypeInfoManager> typeInfoManager;
};

class Loader::Impl {
public:
    Impl() : fileCounter(0) {}

    uint32_t fileCounter;
    std::vector<CbcFile> files;
    std::vector<std::unique_ptr<IO::RandomAccessFile>> rafs;
};

/////////////////////////////////////////////////////////////////
// Session implementation

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

std::tuple<Symlevel::CbcFile&, IO::RandomAccessFile&> Session::File(IO::FileId fileId) const
{
    return { engine.impl->files.at(fileId), *engine.impl->rafs.at(fileId) };
}

Arena& Session::Allocator() { return arena; }

Loader::Loader() : loader(std::move(std::make_unique<Loader::Impl>())) {}

Loader::Loader(Loader&& other) = default;
Loader::~Loader()              = default;

Memory::Heap& Engine::CodeHeap() const { return Memory::Heap::SharedHeap(); }

/////////////////////////////////////////////////////////////////
// Loader implementation

bool Loader::Load(std::unique_ptr<IO::RandomAccessFile> file, std::string_view fileName)
{
    IO::StreamFileReader reader(*file, 0);
    auto id = loader->fileCounter++;
    loader->files.emplace_back(std::move(CbcFile::Create(IO::FileId(id), *file, fileName)));
    loader->rafs.emplace_back(std::move(file));
    return true;
}

Engine& Loader::Build()
{
    auto engineInstance =
        new Engine(std::move(std::make_unique<Engine::Impl>(std::move(loader->files), std::move(loader->rafs))));
    g_engineInstance = engineInstance;
    return *engineInstance;
}

/////////////////////////////////////////////////////////////////
// Engine implementation

Engine::Engine(std::unique_ptr<Engine::Impl>&& impl) : impl(std::move(impl)) {}

Engine::Engine(Engine&& other) = default;
Engine::~Engine()              = default;

Engine& GetEngineInstance()
{
    ASSERTION(g_engineInstance != nullptr, "engine is not initialized");
    // FIXME: load acquire
    return *g_engineInstance;
}

std::optional<CbcFile*> Engine::Impl::FindCbcFile(std::string_view filePath)
{
    for (auto& file : this->files) {
        if (file.GetPath() == filePath) {
            return &file;
        }
    }
    return std::nullopt;
}

std::optional<Identifier<Symlevel::TypeDefinition>> Engine::FindType(Session& session, std::string_view typeName)
{
    for (auto& file : impl->files) {
        auto res = file.GetTypeIndex().FindType(session, typeName);
        if (res.has_value()) {
            return res;
        }
    }

    return std::nullopt;
}

std::vector<Symlevel::CbcFile> const& Engine::Files() const { return impl->files; }

std::optional<Identifier<MethodDefinition>> Engine::FindMain(Session& session, std::string_view filePath)
{
    auto file = impl->FindCbcFile(filePath);
    if (!file.has_value()) {
        return std::nullopt;
    }
    auto f        = file.value();
    auto declType = f->GetTypeIndex().FindType(session, std::string_view("default"));
    if (declType.has_value()) {
        auto type               = Symlevel::TypeDefinition::Resolve(session, declType.value());
        const auto& methodIndex = type.GetMethods();
        auto methods            = methodIndex.FindMethods(session, std::string_view("main"));

        // TODO: throw?
        ASSERTION(methods.size() == 1, "unexpected \"main\" method count");
        return methods[0];
    }
    return std::nullopt;
}

std::optional<Identifier<MethodDefinition>> Engine::FindMethod(
    Session& session, std::string_view filePath, std::string_view typeName, std::string_view methodName
)
{
    auto file = impl->FindCbcFile(filePath);
    if (!file.has_value()) {
        return std::nullopt;
    }
    auto f        = file.value();
    auto declType = f->GetTypeIndex().FindType(session, typeName);
    if (declType.has_value()) {
        auto type               = Symlevel::TypeDefinition::Resolve(session, declType.value());
        const auto& methodIndex = type.GetMethods();
        auto methods            = methodIndex.FindMethods(session, methodName);
        ASSERTION(methods.size() == 1, "unexpected \"main\" method count");
        return methods[0];
    }
    return std::nullopt;
}

} // namespace Engine

/////////////////////////////////////////////////////////////////
// Engine accessors

namespace Interpretation {

FunctionHandleManager& FunctionHandleManager::Of(Engine::Engine& engine)
{
    return Engine::Engine::Impl::Of(engine).fuhManager;
}

FunctionHandleManager& FunctionHandleManager::Of(Engine::Session& session)
{
    return FunctionHandleManager::Of(session.GetEngine());
}

} // namespace Interpretation

namespace Symlevel {

using EngineImpl = Engine::Engine::Impl;

DefinitionsManager& DefinitionsManager::Of(Engine::Engine& engine) { return EngineImpl::Of(engine).defsManager; }

} // namespace Symlevel

namespace Engine {

MethodTableManager& MethodTableManager::Of(Engine& engine) { return *EngineImpl::Of(engine).mtManager; }

MethodTableManager& MethodTableManager::Of(Session& session) { return MethodTableManager::Of(session.GetEngine()); }

TermManager& TermManager::Of(Engine& engine) { return EngineImpl::Of(engine).termManager; }

TermManager& TermManager::Of(Session& session) { return TermManager::Of(session.GetEngine()); }

TypeInfoManager& TypeInfoManager::Of(Engine& engine) { return *EngineImpl::Of(engine).typeInfoManager; }

TypeInfoManager& TypeInfoManager::Of(Session& session) { return TypeInfoManager::Of(session.GetEngine()); }

StaticsManager& StaticsManager::Of(Engine& engine) { return EngineImpl::Of(engine).staticsManager; }

StaticsManager& StaticsManager::Of(Session& session) { return StaticsManager::Of(session.GetEngine()); }

} // namespace Engine
