#pragma once

#include <vector>

#include "io/file_id.h"
#include "io/random_access_file.h"
#include "string.h"
#include "utils/lib_handle.h"

using AotCodeAddr = void*;

namespace Symlevel {

class Dependencies {
public:
    static Dependencies Read(
        IO::FileId fileId,
        IO::RandomAccessFile& file,
        uint32_t poolOffset,
        uint32_t cbcDepsOffset,
        uint32_t aotDepsOffset
    );

    Dependencies(const Dependencies&) = delete;
    Dependencies(Dependencies&& other) noexcept;

    Dependencies& operator=(const Dependencies&)  = delete;
    Dependencies& operator=(Dependencies&& other) = delete;

    AotCodeAddr FindTarget(String linkageName) const;

private:
    Dependencies(std::vector<std::string> cbcDeps, std::vector<LibHandle> handles);

    std::vector<std::string> cbcDeps;
    std::vector<LibHandle> aotHandles;
};

} // namespace Symlevel
