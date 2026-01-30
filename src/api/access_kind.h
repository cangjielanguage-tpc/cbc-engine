#pragma once

#include <cstdint>
#include <string_view>


namespace API {


struct AccessKind {
public:

    enum Value : uint8_t {
        INVALID,
        PUBLIC,
        PRIVATE,
        PROTECTED
    };

    static constexpr int BIT_COUNT = 2;

    constexpr AccessKind(const Value raw) : _value(raw) {}

    constexpr operator Value() const
    {
        return  _value;
    }

    constexpr std::string_view const ToString()
    {
        switch (_value) {
            case INVALID:   return "INVALID";
            case PUBLIC:    return "PUBLIC";
            case PRIVATE:   return "PRIVATE";
            case PROTECTED: return "PROTECTED";
            default:        return "<invalid/unknown>";
        }
    }

private:
    Value _value;
};


} // namespace API
