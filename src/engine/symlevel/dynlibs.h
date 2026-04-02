#pragma once

#include <dlfcn.h>
#include <vector>

#include "io/file_id.h"
#include "io/random_access_file.h"
#include "string.h"

namespace Symlevel {

class Dynlibs {
public:
    static Dynlibs Read(IO::FileId fileId, IO::RandomAccessFile& file, uint32_t offset);
    static Dynlibs Empty();

    explicit Dynlibs(std::vector<void*> handlers);
    ~Dynlibs();

    Dynlibs(const Dynlibs&) = delete;
    Dynlibs(Dynlibs&& other) noexcept;

    Dynlibs& operator=(const Dynlibs&) = delete;
    Dynlibs& operator=(Dynlibs&& other) noexcept;

    void* FindTarget(String linkageName) const;

private:
    std::vector<void*> handlers;
};

} // namespace Symlevel
