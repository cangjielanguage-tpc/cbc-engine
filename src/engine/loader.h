#pragma once

#include "symlevel/io/random_access_file.h"
#include "symlevel/string.h"
#include "engine.h"

namespace Engine {

class Engine {
public:
    ~Engine();

private:
    friend class Loader;
    friend class Session;
    class Impl;

    Engine(Impl* impl) : impl(impl) {}

    Impl* impl;
};

class Loader {
public:
    static Loader New();
    ~Loader();

    bool Load(std::unique_ptr<IO::RandomAccessFile> file, std::string_view name);
    Engine Build();
private:
    class Impl;
    Loader(Impl* loader) : loader(loader) {}

    Impl* loader;
};

} // namespace Engine
