#include "dependencies.h"
#include "io/stream_file_reader.h"

#include <algorithm>
#include <stdio.h>

namespace Symlevel {

std::vector<std::string> Dependencies::parse(IO::FileId fileId, IO::RandomAccessFile& file, uint32_t offset)
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

std::string Dependencies::convertToLibName(const std::string& name)
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
    if (cbcDepsOffset >= 0) {
        cbcDeps = parse(fileId, file, poolOffset + cbcDepsOffset);
    } else {
        cbcDeps = std::vector<std::string>();
    }

    std::vector<LibHandle> handles;
    if (aotDepsOffset >= 0) {
        auto aotDeps = parse(fileId, file, poolOffset + aotDepsOffset);
        handles      = std::vector<LibHandle>(aotDeps.size());

        std::transform(aotDeps.begin(), aotDeps.end(), std::back_inserter(handles), [](std::string dep) {
            std::string libName = convertToLibName(dep);
            LibHandle handle    = dlopen(libName.c_str(), RTLD_LAZY);
            if (!handle) {
                ASSERTION(false, dlerror());
            }
            return handle;
        });
    } else {
        handles = std::vector<LibHandle>();
    }

    return Dependencies(std::move(cbcDeps), std::move(handles));
}

Dependencies::Dependencies(std::vector<std::string> cbcDeps, std::vector<LibHandle> handles)
    : cbcDeps(cbcDeps),
      aotHandles(std::move(handles))
{}

Dependencies::~Dependencies()
{
    for (LibHandle handle : aotHandles) {
        if (!dlclose(handle)) {
            ASSERTION(false, dlerror());
        }
    }
}

Dependencies::Dependencies(Dependencies&& other) noexcept : aotHandles(std::move(other.aotHandles)) {}

Dependencies& Dependencies::operator=(Dependencies&& other) noexcept
{
    if (this != &other) {
        this->aotHandles = std::move(other.aotHandles);
    }
    return *this;
}

AotCodeAddr Dependencies::FindTarget(std::string_view linkageName) const
{
    std::string str(linkageName);

    for (LibHandle handle : aotHandles) {
        AotCodeAddr codeAddr = dlsym(handle, str.c_str());
        if (codeAddr != nullptr) {
            return codeAddr;
        }
    }

    AotCodeAddr codeAddr = dlsym(RTLD_DEFAULT, str.c_str());
    if (codeAddr != nullptr) {
        return codeAddr;
    }

    return nullptr;
}

} // namespace Symlevel
