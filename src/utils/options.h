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
    bool (*setter)(Table const&, Option const&, std::string_view value);
};

class Table {
public:
    using Setter = decltype(Option::setter);

    template <size_t N>
    constexpr Table(Option const (&arr)[N]) : options_(arr), size_(N) {}

    class Snapshot {
        friend class Table;
        struct Entry {
            size_t index;
            std::string data;
        };
        std::vector<Entry> entries;
    };

    enum class Status { OK, UNKNOWN_OPTION, INVALID_OPTION };
    Status Set(std::string_view key, std::string_view value) const;

    Snapshot SaveContext() const;
    void RestoreContext(Snapshot const&) const;

    size_t Size() const { return size_; }
    Option const& operator[](size_t i) const { return options_[i]; }

private:
    Option const* options_;
    size_t size_;
};

void ParseAndSet(int size, char const** optStr, Table const& opts);
void InitFromString(std::string_view optStr, Table const& opts);
void InitFromEnv(Table const& opts);

extern Table const g_table;

} // namespace Options
