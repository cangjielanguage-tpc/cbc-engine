#include "dependencies.h"
#include "io/stream_file_reader.h"

#include <algorithm>
#include <stdio.h>

namespace Symlevel {

std::vector<std::string> ReadDependencies(IO::FileId fileId, IO::RandomAccessFile& file, uint32_t offset)
{
    IO::StreamFileReader reader(file, offset);

    uint32_t size = reader.ReadULEB();

    std::vector<char> buffer(size);
    reader.Read(buffer.data(), size);

    std::vector<std::string> results;

    std::string_view sv(buffer.data(), size);
    char delim   = ':';
    size_t start = 0;
    while (start < sv.size()) {
        size_t end = sv.find(delim, start);

        std::string_view token = sv.substr(start, end - start);
        if (!token.empty()) {
            results.push_back(std::string(token));
        }

        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }

    return results;
}

std::string ConvertToLibName(const std::string& name)
{
#if defined(_WIN32) || defined(_WIN64)
    return name + ".dll";
#elif defined(__APPLE__)
    return "lib" + name + ".dylib";
#else
    return "lib" + name + ".so";
#endif
}

Dependencies Dependencies::Read(
    IO::FileId fileId, IO::RandomAccessFile& file, uint32_t poolOffset, uint32_t cbcDepsOffset, uint32_t aotDepsOffset
)
{
    std::vector<std::string> cbcDeps;
    if (cbcDepsOffset != 0) {
        cbcDeps = ReadDependencies(fileId, file, poolOffset + cbcDepsOffset);
    } else {
        cbcDeps = std::vector<std::string>();
    }

    std::vector<LibHandle> handles;
    if (aotDepsOffset != 0) {
        auto aotDeps = ReadDependencies(fileId, file, poolOffset + aotDepsOffset);
        handles.reserve(aotDeps.size());
        for (const auto& dep : aotDeps) {
            handles.emplace_back(ConvertToLibName(dep));
        }
    } else {
        handles = std::vector<LibHandle>();
    }

    return Dependencies(std::move(cbcDeps), std::move(handles));
}

Dependencies::Dependencies(std::vector<std::string> cbcDeps, std::vector<LibHandle> handles)
    : cbcDeps(cbcDeps),
      aotHandles(std::move(handles))
{}

Dependencies::Dependencies(Dependencies&& other) noexcept : aotHandles(std::move(other.aotHandles)) {}

AotCodeAddr Dependencies::FindTarget(String linkageName) const
{
    for (const LibHandle& handle : aotHandles) {
        AotCodeAddr codeAddr = handle.FindTarget(std::string(linkageName));
        if (codeAddr != nullptr) {
            return codeAddr;
        }
    }

    ASSERTION(false, "Dependencies: cannot find target lib for method");
    return nullptr;
}

} // namespace Symlevel
