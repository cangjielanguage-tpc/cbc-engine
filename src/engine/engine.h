#pragma once

#include "arena.h"
#include "symlevel/cbc_file.h"
#include "symlevel/io/file_id.h"
#include "symlevel/io/random_access_file.h"

namespace Engine {

/// An opaque handle to symlevel definitions.
using MethodDefIdentifier = void*;
using TypeDefIdentifier   = void*;

class Loader;

/// The main instance of cbc engine.
/// Mainly the engine is responsible for:
/// - storage of the information about loaded cbc files;
/// - (TODO) locating the resources, and providing access to cbc-defined types;
class Engine {
public:
    ~Engine();
    std::pmr::memory_resource& CodeHeap() const;

private:
    friend class Loader;
    friend class Session;
    class Impl;

    Engine(std::unique_ptr<Impl>&& impl);
    Engine(Engine&& other);

    std::unique_ptr<Impl> impl;
};

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

    bool Load(std::unique_ptr<IO::RandomAccessFile> file, std::string_view name);
    Engine Build();
private:
    class Impl;

    std::unique_ptr<Impl> loader;
};

} // namespace Engine
