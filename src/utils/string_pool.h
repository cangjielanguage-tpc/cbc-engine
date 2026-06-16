#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Utils {

class StringPool {
public:
    struct ZeroTerminatedView : public std::string_view {
        friend class StringPool;

    private:
        ZeroTerminatedView(std::string const& str);
    };

    ZeroTerminatedView Intern(std::string_view str);
    size_t InternAndGetId(std::string_view str);
    ZeroTerminatedView GetStringById(size_t id);

private:
    std::unordered_map<std::string_view, size_t> map;
    std::vector<std::string> strings;
};

}
