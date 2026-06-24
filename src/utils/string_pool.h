#pragma once

#include <string_view>
#include <unordered_map>
#include <vector>

namespace Utils {

class StringPool {
public:
    struct String { // std::string has small-string optimization
        char* str;
        size_t size;

        operator std::string_view();
    };

    String Intern(std::string_view str);
    size_t InternAndGetId(std::string_view str);
    String GetStringById(size_t id);

private:
    std::unordered_map<std::string_view, size_t> map;
    std::vector<String> strings;
};

}
