
#include "engine.h"
#include <string_view>

namespace Engine {

namespace {

constexpr size_t RESOLVE_ERR_BUFFER_SIZE = 256;

} // namespace

void* Dependencies::FindSymbol(std::string_view linkageName) const
{
    std::string name(linkageName);
    return FindSymbol(name.c_str());
}

void* Dependencies::FindSymbol(char const* linkageName) const
{
    for (auto& handle : objects) {
        auto sym = handle->SearchSym(linkageName).symbol;
        if (sym)
            return sym;
    }

    if (resolverFn != nullptr) {
        char errBuffer[RESOLVE_ERR_BUFFER_SIZE];
        return resolverFn(resolverCtx, linkageName, errBuffer);
    }
    return nullptr;
}

} // namespace Engine
