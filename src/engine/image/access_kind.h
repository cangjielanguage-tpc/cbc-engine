#pragma once

#include <cstdint>
#include <string_view>

namespace Image {

struct AccessKind {
public:
    enum Value : uint8_t {
        INVALID,
        PUBLIC,
        PRIVATE,
        PROTECTED
    };

    static constexpr int BIT_COUNT = 2;

    constexpr AccessKind(const Value value) : value(value) {}

    constexpr operator Value() const { return value; }

    constexpr std::string_view const ToString()
    {
        switch (value) {
            case INVALID:   return "INVALID";
            case PUBLIC:    return "PUBLIC";
            case PRIVATE:   return "PRIVATE";
            case PROTECTED: return "PROTECTED";
            default:        return "<invalid/unknown>";
        }
    }

private:
    Value value;
};

} // namespace Image
