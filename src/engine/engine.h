#pragma once

#include "arena.h"
#include "symlevel/io/file_id.h"
#include "symlevel/io/random_access_file.h"

namespace Engine {

class Engine;

class Session {
public:
    IO::RandomAccessFile* FileOf(IO::FileId fileId);

    static Session NewSession(Engine& engine);
    Arena& Allocator();

private:
    Session(Engine& engine) : engine(engine), arena() {}

    Arena arena;
    Engine& engine;
};

} // namespace Engine
