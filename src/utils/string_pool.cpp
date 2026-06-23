#include "string_pool.h"
#include "assertion.h"
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace Utils {

StringPool::String StringPool::Intern(std::string_view str)
{
    return strings[InternAndGetId(str)];
}

size_t StringPool::InternAndGetId(std::string_view str)
{
    auto it = map.find(str);
    if (it != map.end()) {
        auto id = it->second;
        return id;
    }
    auto id = strings.size();

    char *mem = (char*) malloc(str.size() + 1);
    if (mem == nullptr) {
        FATAL("out of memory");
    }
    memcpy(mem, str.data(), str.size());
    mem[str.size()] = 0;

    String s{mem, str.size()};
    strings.push_back(s);
    map.insert_or_assign(s, id);
    return id;
}

StringPool::String StringPool::GetStringById(size_t id)
{
    return strings.at(id);
}

StringPool::String::operator std::string_view() {
    return std::string_view(str, size);
}

}
