#include "dynlibs.h"
#include "io/stream_file_reader.h"

namespace Symlevel {

Dynlibs Dynlibs::Read(IO::FileId fileId, IO::RandomAccessFile& file, uint32_t offset)
{
    IO::StreamFileReader reader(file, offset);

    uint32_t size = reader.ReadULEB();

    std::vector<char> dynlibs(size);
    reader.Read(dynlibs.data(), size);

    std::vector<void*> handlers;

    std::string_view sv(dynlibs.data(), size);
    char delim   = ':';
    size_t start = 0;

    while (start < sv.size()) {
        size_t end = sv.find(delim, start);

        std::string_view element = sv.substr(start, end - start);
        if (!element.empty()) {
            std::string dynlib(element);

            void* handler = dlopen(dynlib.c_str(), RTLD_LAZY);
            if (!handler) {
                ASSERTION(handler != nullptr, "cannot open lib");
            }
            handlers.push_back(handler);
        }

        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }

    return Dynlibs(std::move(handlers));
}

Dynlibs Dynlibs::Empty() { return Dynlibs({}); }

Dynlibs::Dynlibs(std::vector<void*> handlers) : handlers(std::move(handlers)) {}

Dynlibs::~Dynlibs()
{
    for (void* handler : handlers) {
        if (handler) {
            dlclose(handler);
        }
    }
}

Dynlibs::Dynlibs(Dynlibs&& other) noexcept : handlers(std::move(other.handlers)) {}

Dynlibs& Dynlibs::operator=(Dynlibs&& other) noexcept
{
    if (this != &other) {
        for (void* handler : handlers)
            if (handler) {
                dlclose(handler);
            }
        handlers = std::move(other.handlers);
    }
    return *this;
}

void* Dynlibs::FindTarget(String linkageName) const
{
    std::string name(linkageName);
    for (void* handler : handlers) {
        void* target = dlsym(handler, name.c_str());
        if (target != nullptr) {
            return target;
        }
    }
    ASSERTION(false, "cannot find target");
    return nullptr;
}

} // namespace Symlevel
