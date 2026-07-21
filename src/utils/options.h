#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace Options {

class Table;

struct Option {
    std::string_view name;
    void* location;
    bool (*setter)(const Table&, const Option&, std::string_view value);
};

class Table {
public:
    using Setter = decltype(Option::setter);

    template <size_t N> constexpr Table(Option const (&arr)[N]) : options_(arr), size_(N) {}

    const Option& operator[](size_t i) const { return options_[i]; }

    enum class Status {
        OK,
        UNKNOWN_OPTION,
        INVALID_OPTION
    };

    Status Set(std::string_view key, std::string_view value) const;
    void ParseAndSet(int size, const char* const* optStr) const;
    size_t Size() const { return size_; }

private:
    Option const* options_;
    size_t size_;
};

void InitFromString(std::string_view optStr, const Table& opts);
void InitFromEnv(const Table& opts);

bool SetLogLevelValue(Table const&, Option const&, std::string_view value);
bool SetBoolValue(Table const&, Option const&, std::string_view value);
bool SetIntValue(Table const&, Option const&, std::string_view value);
bool SetStringValue(Table const&, Option const&, std::string_view value);
bool SetAllLogLevels(Table const&, Option const&, std::string_view value);

} // namespace Options
