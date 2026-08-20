#include "engine.h"
#include "decode/decoder.h"
#include "engine/image/io/random_access_file.h"
#include "engine/method_table.h"
#include "engine/statics_manager.h"
#include "engine/terms.h"
#include "engine/typeinfo_manager.h"
#include "image/cbc_file.h"
#include "image/io/stream_file_reader.h"
#include "image/reader.h"
#include "interpreter/function_handle.h"
#include "utils/assertion.h"
#include "utils/heap.h"
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string_view>

namespace Engine {

using namespace Image;

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

std::tuple<Image::CbcFile&, IO::RandomAccessFile&> Session::File(FileId fileId) const
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

std::optional<CbcFile> TryReadCbcFile(Image::FileId fileId, IO::RandomAccessFile* file, std::string_view name)
{
    // TODO: file verification is required.
    IO::StreamFileReader reader(file, 0);
    static const uint32_t FILE_VERSION_SHIFT = 24;
    static constexpr auto MAGIC              = "CBC\x01";

    char magic[4];
    auto fileLength = file->FileLength();
    if (fileLength < sizeof(magic)) {
        return std::nullopt;
    }
    reader.Read(magic, sizeof(magic));

    if (memcmp(magic, MAGIC, sizeof(magic)) != 0)
        return std::nullopt;
    // TODO: file version checks

    // Do not bother with verification further.

    reader.Advance(2); // skip bytecode version and file props (TODO: change file format)

    auto typeIndexOffset = reader.ReadU32();
    auto poolOffset      = reader.ReadU32();

    auto directCallAotTableOffset    = reader.ReadU32();
    auto virtualCallAotTableOffset   = reader.ReadU32();
    auto interfaceCallAotTableOffset = reader.ReadU32();
    auto staticFieldAotTableOffset   = reader.ReadU32();
    auto instanceFieldAotTableOffset = reader.ReadU32();

    reader.Advance(2); // skip region number
    auto regionOffset = reader.ReadU32();

    std::optional<Identifier<String>> mainTypeName = std::nullopt;
    if (auto mainType = reader.ReadS32(); mainType >= 0) {
        mainTypeName = Image::Identifier(Offset<String>(mainType), fileId);
    }

    auto cbcDeps     = reader.ReadS32();
    auto aotDeps     = reader.ReadS32();
    auto foreignLibs = reader.ReadS32();
    auto coverageId  = reader.ReadULEB();

    IO::StreamFileReader typeIndexReader(file, typeIndexOffset);
    IO::StreamFileReader directCallTableReader(file, directCallAotTableOffset);
    IO::StreamFileReader virtualCallTableReader(file, virtualCallAotTableOffset);
    IO::StreamFileReader interfaceCallTableReader(file, interfaceCallAotTableOffset);
    IO::StreamFileReader instanceFieldTableReader(file, instanceFieldAotTableOffset);
    IO::StreamFileReader staticFieldTableReader(file, staticFieldAotTableOffset);

    return Image::CbcFile {
        .typeIndex             = Decode::ReadIndex(typeIndexReader, fileId),
        .regionData            = Decode::ReadRegion(fileId, *file, regionOffset),
        .directCallAotTable    = Decode::ReadIndex(directCallTableReader, fileId),
        .virtualCallAotTable   = Decode::ReadIndex(virtualCallTableReader, fileId),
        .interfaceCallAotTable = Decode::ReadIndex(interfaceCallTableReader, fileId),
        .staticFieldAotTable   = Decode::ReadIndex(staticFieldTableReader, fileId),
        .instanceFieldAotTable = Decode::ReadIndex(instanceFieldTableReader, fileId),
        .aotDeps               = aotDeps,
        .cbcDeps               = cbcDeps,
        .mainTypeName          = mainTypeName,
        .poolOffset            = poolOffset,
        .id                    = fileId,
        .name                  = std::string(name),
    };
}

bool Loader::Load(std::unique_ptr<IO::RandomAccessFile> file, std::string_view fileName)
{
    IO::StreamFileReader reader(*file, 0);
    auto id = loader->fileCounter;
    auto f  = TryReadCbcFile(FileId(id), file.get(), fileName);
    if (f) {
        loader->files.emplace_back(std::move(*f));
        loader->rafs.emplace_back(std::move(file));
        return true;
    }
    return false;
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

    // FIXME: Do not inject `executable` as dependency unconditionally.
    //        Use special name for such dependencies as `aot deps` field in cbc file.
    auto executable = std::make_shared<Utils::SharedObject>(Utils::SharedObject::OpenCurrentExecutable());

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

        std::vector<std::shared_ptr<Utils::SharedObject>> ptrs;
        ptrs.emplace_back(executable);
        if (!deps) {
            allDeps.emplace_back(std::move(ptrs));
            continue;
        }

        IO::StreamFileReader reader(raf, *deps + POOL_OFFSET_ADJUSTMENT);

        uint32_t size = reader.ReadULEB();
        nameBuffer.clear();
        nameBuffer.resize(size);
        reader.Read(nameBuffer.data(), size);

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

std::optional<Identifier<Image::TypeDefinition>> Engine::FindType(Session& session, std::string_view typeName)
{
    for (auto& file : impl->files) {
        auto res = Reader::Find(session, file.GetTypeIndex(), typeName);
        if (res.has_value()) {
            return res;
        }
    }

    return std::nullopt;
}

std::vector<Image::CbcFile> const& Engine::Files() const { return impl->files; }

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
    auto declType = Reader::Find(session, f->GetTypeIndex(), typeName);
    if (declType.has_value()) {
        auto type               = Decode::Read(session, declType.value());
        const auto& methodIndex = type.GetMethods();

        std::optional<Identifier<MethodDefinition>> result = std::nullopt;
        int mcount                                         = 0;
        for (auto m : Reader::FindBucket(session, methodIndex, methodName)) {
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
    auto type = Decode::Read(session, *mainTypeName);
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

namespace Image {

using EngineImpl = Engine::Engine::Impl;

} // namespace Image

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
