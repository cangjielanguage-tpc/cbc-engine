#pragma once

#include "symlevel/io/random_access_file.h"
#include "symlevel/string.h"
#include "engine.h"

namespace Engine {

class Loader {
public:
    static Loader New();

    bool Load(std::unique_ptr<IO::RandomAccessFile> file, std::string_view name);
    Engine Build();

    ~Loader();
private:
    class Impl;
    Loader(Impl* loader) : loader(loader) {}

    Impl* loader;
};

} // namespace Engine
