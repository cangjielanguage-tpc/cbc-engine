#include "engine.h"
#include "decode/decoder.h"
#include "engine/method_table.h"
#include "engine/statics_manager.h"
#include "engine/symlevel/io/random_access_file.h"
#include "engine/terms.h"
#include "engine/typeinfo_manager.h"
#include "interpreter/function_handle.h"
#include "symlevel/cbc_file.h"
#include "symlevel/io/stream_file_reader.h"
#include "symlevel/reader.h"
#include "utils/assertion.h"
#include "utils/heap.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

namespace Engine {

using namespace Symlevel;

static Engine* g_engineInstance;

/////////////////////////////////////////////////////////////////
// Impl definitions

class Engine::Impl {
public:
    Impl(
        std::vector<CbcFile>&& files,
        std::vector<std::unique_ptr<IO::RandomAccessFile>>&& rafs,
        std::vector<class Dependencies>&& deps
    )
        : files(std::move(files)),
          rafs(std::move(rafs)),
          dependencies(std::move(deps)),
          typeInfoManager(TypeInfoManager::NewInstance()),
          mtManager(MethodTableManager::NewInstance())
    {}

    static Engine::Impl& Of(Engine& engine) { return *engine.impl; }

    std::optional<CbcFile*> FindCbcFile(std::string_view filePath);

    friend class Session;
    std::vector<CbcFile> files;
    std::vector<std::unique_ptr<IO::RandomAccessFile>> rafs;

    Interpretation::FunctionHandleManager fuhManager;
    std::unique_ptr<MethodTableManager> mtManager;
    TermManager termManager;
    StaticsManager staticsManager;
    std::unique_ptr<TypeInfoManager> typeInfoManager;
    std::vector<class Dependencies> dependencies;
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
Session::Session(Engine& engine) : engine(engine), arena() { decoder = new Decode::Decoder(*this); }

Session::~Session() { delete decoder; }

std::unique_ptr<IO::RandomAccessFile>& Session::FileOf(FileId fileId) const
{
    // TODO: add session-scoped buffered rafs.
    return engine.impl->rafs.at(fileId);
}

CbcFile& Session::CbcFileOf(FileId fileId) const
{
    // TODO: add session-scoped buffered rafs.
    return engine.impl->files.at(fileId);
}

std::tuple<Symlevel::CbcFile&, IO::RandomAccessFile&> Session::File(FileId fileId) const
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
    loader->files.emplace_back(std::move(CbcFile::Create(FileId(id), *file, fileName)));
    loader->rafs.emplace_back(std::move(file));
    return true;
}

static std::string UpdateSharedObjName(std::string_view name)
{
    std::string res;
#if defined(_WIN32) || defined(_WIN64)
    res.reserve(name.size() + 4);
    res += name;
    res += ".dll";
#elif defined(__APPLE__)
    res.reserve(name.size() + 9);
    res += "lib";
    res += name;
    res += ".dylib";
#else
    res.reserve(name.size() + 6);
    res += "lib";
    res += name;
    res += ".so";
    return res;
#endif
}

static std::vector<Dependencies> ReadDependencies(Loader::Impl const* loader)
{
    static constexpr char delim = ':';

    std::vector<std::shared_ptr<Utils::SharedObject>> objects;
    auto addObject = [&objects](std::string_view name) {
        auto soName = UpdateSharedObjName(name);
        // Avoid duplicate dlopen calls
        for (auto& obj : objects) {
            if (soName.compare(obj->Name()) == 0) {
                return obj;
            }
        }
        auto ptr = std::make_shared<Utils::SharedObject>(Utils::SharedObject::Open(std::move(soName)));
        objects.emplace_back(ptr);
        return ptr;
    };

    std::vector<char> nameBuffer;

    ASSERT(loader->files.size() == loader->rafs.size());
    auto sz = loader->files.size();
    std::vector<Dependencies> allDeps;
    allDeps.reserve(sz);

    for (size_t i = 0; i < sz; i++) {
        auto raf   = loader->rafs[i].get();
        auto& file = loader->files[i];

        auto deps     = file.AotDependencies();
        auto fileDeps = &allDeps[i];
        if (!deps) {
            allDeps.emplace_back();
            continue;
        }

        IO::StreamFileReader reader(raf, *deps + POOL_OFFSET_ADJUSTMENT);

        uint32_t size = reader.ReadULEB();
        nameBuffer.clear();
        nameBuffer.resize(size);
        reader.Read(nameBuffer.data(), size);

        std::vector<std::shared_ptr<Utils::SharedObject>> ptrs;

        std::string_view depsStr(nameBuffer.data(), size);
        while (!depsStr.empty()) {
            auto pos = depsStr.find(delim);
            if (pos == std::string_view::npos) {
                ptrs.emplace_back(addObject(depsStr));
                break;
            } else {
                auto token = depsStr.substr(0, pos);
                ptrs.emplace_back(addObject(token));
                depsStr = depsStr.substr(pos + 1);
            }
        }
        allDeps.emplace_back(std::move(ptrs));
    }

    return allDeps;
}

Engine& Loader::Build()
{
    auto deps = ReadDependencies(loader.get());

    auto engineInstance = new Engine(
        std::move(std::make_unique<Engine::Impl>(std::move(loader->files), std::move(loader->rafs), std::move(deps)))
    );
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
        auto res = session.Decoder().Find(file.GetTypeIndex(), typeName);
        if (res.has_value()) {
            return res;
        }
    }

    return std::nullopt;
}

std::vector<Symlevel::CbcFile> const& Engine::Files() const { return impl->files; }

std::vector<Dependencies> const& Engine::Dependencies() const { return impl->dependencies; }

std::optional<Identifier<MethodDefinition>> Engine::FindMethod(
    Session& session, std::string_view filePath, std::string_view typeName, std::string_view methodName
)
{
    auto file = impl->FindCbcFile(filePath);
    if (!file.has_value()) {
        return std::nullopt;
    }
    auto f        = file.value();
    auto declType = session.Decoder().Find(f->GetTypeIndex(), typeName);
    if (declType.has_value()) {
        auto type               = Symlevel::Reader::Read(session, declType.value());
        const auto& methodIndex = type.GetMethods();

        std::optional<Identifier<MethodDefinition>> result = std::nullopt;
        int mcount                                         = 0;
        for (auto m : session.Decoder().FindBucket(methodIndex, methodName)) {
            mcount++;
            result = m;
        }
        ASSERTION(mcount == 1, "unexpected method count");
        return result;
    }
    return std::nullopt;
}

std::optional<Identifier<MethodDefinition>> Engine::FindMain(Session& session, std::string_view filePath)
{
    auto file = impl->FindCbcFile(filePath);
    if (!file.has_value()) {
        return std::nullopt;
    }
    auto f        = file.value();
    auto mainTypeName = f->GetMainTypeName();
    if (!mainTypeName.has_value()) {
        return std::nullopt;
    }
    auto type = Symlevel::Reader::Read(session, *mainTypeName);
    return FindMethod(session, filePath, type, "main");
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
