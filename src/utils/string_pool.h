#pragma once

#include "utils/vector.h"
#include <string_view>
#include <unordered_map>

namespace Utils {

class StringPool {
public:
    struct String { // std::string has small-string optimization
        char* str;
        size_t size;

        operator std::string_view();
    };

    StringPool() = default;
    ~StringPool();
    // map keys are string_views into owned memory, copying would lead to double free
    StringPool(const StringPool&)            = delete;
    StringPool& operator=(const StringPool&) = delete;

    String Intern(std::string_view str);
    size_t InternAndGetId(std::string_view str);
    String GetStringById(size_t id);

private:
    std::unordered_map<std::string_view, size_t> map;
    Utils::Vector<String> strings;
};

} // namespace Utils
