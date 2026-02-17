#pragma once

#include <functional>
#include "arena.h"
#include "symlevel/cbc_file.h"
#include "symlevel/io/file_id.h"
#include "symlevel/io/random_access_file.h"

namespace Engine {

class Engine;

class Session {
public:
    std::unique_ptr<IO::RandomAccessFile>& FileOf(IO::FileId fileId) const;
    Symlevel::CbcFile& CbcFileOf(IO::FileId fileId) const;

    static Session NewSession(Engine& engine);
    Arena& Allocator();

private:
    Session(Engine& engine) : engine(engine), arena() {}

    Arena arena;
    Engine& engine;
};

} // namespace Engine
