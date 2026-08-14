#pragma once

#include <dlfcn.h>
#include <vector>

#include "io/file_id.h"
#include "io/random_access_file.h"

using LibHandle   = void*;
using AotCodeAddr = void*;

namespace Symlevel {

class Dependencies {
public:
    static Dependencies Read(
        IO::FileId fileId,
        IO::RandomAccessFile& file,
        int32_t poolOffset,
        int32_t cbcDepsOffset,
        int32_t aotDepsOffset
    );

    explicit Dependencies(std::vector<std::string> cbcDeps, std::vector<LibHandle> handles);
    ~Dependencies();

    Dependencies(const Dependencies&) = delete;
    Dependencies(Dependencies&& other) noexcept;

    Dependencies& operator=(const Dependencies&) = delete;
    Dependencies& operator=(Dependencies&& other) noexcept;

    AotCodeAddr FindTarget(std::string_view linkageName) const;

private:
    std::vector<std::string> cbcDeps;
    std::vector<LibHandle> aotHandles;

    static std::string convertToLibName(const std::string& name);
    static std::vector<std::string> parse(IO::FileId fileId, IO::RandomAccessFile& file, uint32_t offset);
};

} // namespace Symlevel
