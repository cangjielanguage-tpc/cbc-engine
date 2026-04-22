#pragma once

#include <optional>

#include "arena.h"
#include "identifiers.h"
#include "symlevel/cbc_file.h"
#include "symlevel/io/file_id.h"
#include "symlevel/io/random_access_file.h"
#include "utils/heap.h"

namespace Engine {

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

    std::optional<Identifier<MethodDefinition>> FindMethod(Session& session, std::string_view filePath, std::string_view typeName, std::string_view methodName);
    std::optional<TypeDefinition> FindType(Session& session, std::string_view typeName);

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
    std::unique_ptr<IO::RandomAccessFile>& FileOf(IO::FileId fileId) const;
    Symlevel::CbcFile& CbcFileOf(IO::FileId fileId) const;

    Session(Engine& engine) : engine(engine), arena() {}

    Engine& GetEngine() const { return engine; }

    Arena& Allocator();

private:
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

private:
    class Impl;

    std::unique_ptr<Impl> loader;
};

} // namespace Engine
