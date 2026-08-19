#pragma once

#include <optional>

#include "arena.h"
#include "identifiers.h"
#include "symlevel/cbc_file.h"
#include "symlevel/io/random_access_file.h"
#include "utils/heap.h"
#include "utils/sharedobj.h"

namespace Decode {
struct Decoder;
};

namespace Engine {

using FileId = Symlevel::FileId;

/// List of dependencies that engine is using.
class Dependencies {
    using SharedObject = Utils::SharedObject;

public:
    Dependencies(std::vector<std::shared_ptr<SharedObject>>&& objects) : objects(std::move(objects)) {}

    Dependencies() = default;

    /// Return symbol's pointer or null on error.
    void* FindSymbol(std::string_view linkageName) const;
    void* FindSymbol(char const* linkageName) const;

private:
    std::vector<std::shared_ptr<SharedObject>> objects;
};

class Loader;
class Session;

/// The main instance of cbc engine.
/// Mainly the engine is responsible for:
/// - storage of the information about loaded cbc files;
/// - (TODO) locating the resources, and providing access to cbc-defined types;
class Engine {
public:
    using MethodDefinition = Symlevel::MethodDefinition;
    using TypeDefinition   = Symlevel::TypeDefinition;

    class Impl;
    friend class Loader;
    friend class Session;
    friend class Impl;

    ~Engine();
    Memory::Heap& CodeHeap() const;

    std::optional<Identifier<MethodDefinition>> FindMain(Session& session, std::string_view filePath);
    std::optional<Identifier<MethodDefinition>> FindMethod(
        Session& session, std::string_view filePath, std::string_view typeName, std::string_view methodName
    );
    std::optional<Identifier<TypeDefinition>> FindType(Session& session, std::string_view typeName);

    std::vector<Symlevel::CbcFile> const& Files() const;
    std::vector<Dependencies> const& Dependencies() const;

private:
    Engine(std::unique_ptr<Impl>&& impl);
    Engine(Engine&& other);

    std::unique_ptr<Impl> impl;
};

Engine& GetEngineInstance();

/// Temporal context for `Engine` usage.
/// Almost all accesses to the engine is performed in the presence of `Session`.
class Session {
public:
    std::unique_ptr<IO::RandomAccessFile>& FileOf(FileId fileId) const;
    Symlevel::CbcFile& CbcFileOf(FileId fileId) const;
    std::tuple<Symlevel::CbcFile&, IO::RandomAccessFile&> File(FileId fileId) const;

    Session(Engine& engine);
    ~Session();

    Engine& GetEngine() const { return engine; }

    Arena& Allocator();

    Decode::Decoder& Decoder() const { return *decoder; }

private:
    Decode::Decoder* decoder;
    Arena arena;
    Engine& engine;
};

/// Builder of an engine.
class Loader {
public:
    Loader();
    Loader(Loader&& other);
    ~Loader();

    bool Load(std::unique_ptr<IO::RandomAccessFile> file, std::string_view fileName);
    Engine& Build();

    class Impl;

private:
    std::unique_ptr<Impl> loader;
};

} // namespace Engine
