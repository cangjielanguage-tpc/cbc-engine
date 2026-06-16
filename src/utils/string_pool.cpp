#include "string_pool.h"

namespace Utils {
using ZView = StringPool::ZeroTerminatedView;

ZView::ZeroTerminatedView(std::string const& str) : std::string_view(str) {}

ZView StringPool::Intern(std::string_view str)
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
    strings.emplace_back(str);
    auto view = ZeroTerminatedView(strings[id]);
    map.emplace(view, id);
    return id;
}

ZView StringPool::GetStringById(size_t id)
{
    return strings.at(id);
}

}
