#pragma once

#include "symlevel/io/random_access_file.h"
#include "symlevel/string.h"
#include "engine.h"

namespace Engine {

class _Loader;

class Loader {
public:
    static Loader New();

    bool Load(std::unique_ptr<IO::RandomAccessFile> file, std::string_view name);
    Engine* Build();
private:
    Loader(std::unique_ptr<_Loader> loader) : loader(std::move(loader)) {}

    std::unique_ptr<_Loader> loader;
};

} // namespace Engine
