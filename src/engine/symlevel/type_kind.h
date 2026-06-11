#pragma once

#include <cstdint>
#include <string_view>

namespace Symlevel {

struct TypeKind {
public:
    enum Value : uint32_t {
        CLASS,
        INTERFACE,
        LAMBDA,
        RECORD
    };

    static constexpr int BIT_COUNT = 3;

    constexpr TypeKind(const Value value) : value(value) {};

    constexpr operator Value() const { return value; }

    constexpr std::string_view const ToString()
    {
        switch (value) {
            case CLASS:     return "CLASS";
            case INTERFACE: return "INTERFACE";
            case RECORD:    return "RECORD";
            default:        return "<unknown>";
        }
    }

private:
    const Value value;
};

} // namespace Symlevel
