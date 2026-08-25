
#include "engine.h"
#include <string_view>

namespace Engine {

void* Dependencies::FindSymbol(std::string_view linkageName) const
{
    std::string name(linkageName);
    return FindSymbol(name.c_str());
}

void* Dependencies::FindSymbol(char const* linkageName) const
{
    for (auto& handle : objects) {
        auto sym = handle->SearchSym(linkageName);
        if (sym)
            return sym;
    }
    return nullptr;
}

} // namespace Engine
