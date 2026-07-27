#include "dependencies.h"
#include "io/stream_file_reader.h"

#include <algorithm>
#include <stdio.h>

namespace Symlevel {

Utils::Vector<std::string> Dependencies::parse(IO::FileId fileId, IO::RandomAccessFile& file, uint32_t offset)
{
    IO::StreamFileReader reader(file, offset);

    uint32_t size = reader.ReadULEB();

    Utils::Vector<char> buffer(size);
    reader.Read(buffer.data(), size);

    Utils::Vector<std::string> results;

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
    IO::FileId fileId, IO::RandomAccessFile& file, int32_t poolOffset, int32_t cbcDepsOffset, int32_t aotDepsOffset
)
{
    Utils::Vector<std::string> cbcDeps;
    if (cbcDepsOffset >= 0) {
        cbcDeps = parse(fileId, file, poolOffset + cbcDepsOffset);
    } else {
        cbcDeps = Utils::Vector<std::string>();
    }

    Utils::Vector<LibHandle> handles;
    if (aotDepsOffset >= 0) {
        auto aotDeps = parse(fileId, file, poolOffset + aotDepsOffset);
        handles.reserve(aotDeps.size());

        for (auto& dep : aotDeps) {
            std::string libName = convertToLibName(dep);
            LibHandle handle    = dlopen(libName.c_str(), RTLD_LAZY);
            if (!handle) {
                ASSERTION(false, dlerror());
            }
            handles.emplace_back(handle);
        }
    } else {
        handles = Utils::Vector<LibHandle>();
    }

    return Dependencies(std::move(cbcDeps), std::move(handles));
}

Dependencies::Dependencies(Utils::Vector<std::string>&& cbcDeps, Utils::Vector<LibHandle>&& handles)
    : cbcDeps(std::move(cbcDeps)),
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

// TODO: use optional
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
