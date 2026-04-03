#pragma once

#include <cstdio>
#include <string>

namespace Stream {

class endl_t {};
constexpr endl_t endl;

class Out {
public:
    FILE* file;

    Out(FILE* stream_file) : file(stream_file) {}

    const Out& operator<<(const endl_t&) const
    {
        fprintf(file, "\n");
        return *this;
    }

    const Out& operator<<(const float v) const
    {
        fprintf(file, "%f", v);
        return *this;
    }

    const Out& operator<<(const double v) const
    {
        fprintf(file, "%f", v);
        return *this;
    }

    const Out& operator<<(const long double v) const
    {
        fprintf(file, "%Lf", v);
        return *this;
    }

    const Out& operator<<(const bool v) const
    {
        fprintf(file, "%s", v ? "true" : "false");
        return *this;
    }

    const Out& operator<<(const signed char v) const
    {
        fprintf(file, "%d", v);
        return *this;
    }

    const Out& operator<<(const unsigned char v) const
    {
        fprintf(file, "%u", v);
        return *this;
    }

    const Out& operator<<(const short v) const
    {
        fprintf(file, "%d", v);
        return *this;
    }

    const Out& operator<<(const unsigned short v) const
    {
        fprintf(file, "%u", v);
        return *this;
    }

    const Out& operator<<(const int v) const
    {
        fprintf(file, "%d", v);
        return *this;
    }

    const Out& operator<<(const unsigned int v) const
    {
        fprintf(file, "%u", v);
        return *this;
    }

    const Out& operator<<(const long v) const
    {
        fprintf(file, "%ld", v);
        return *this;
    }

    const Out& operator<<(const unsigned long v) const
    {
        fprintf(file, "%ld", v);
        return *this;
    }

    const Out& operator<<(const long long v) const
    {
        fprintf(file, "%lld", v);
        return *this;
    }

    const Out& operator<<(const unsigned long long v) const
    {
        fprintf(file, "%llu", v);
        return *this;
    }

    const Out& operator<<(const char* cstr) const
    {
        fprintf(file, "%s", cstr);
        return *this;
    }

    const Out& operator<<(const char c) const
    {
        fprintf(file, "%c", c);
        return *this;
    }

    const Out& operator<<(const ::std::string_view strv) const
    {
        fprintf(file, "%.*s", static_cast<int>(strv.length()), strv.data());
        return *this;
    }

    const Out& operator<<(const ::std::string str) const
    {
        fprintf(file, "%s", str.c_str());
        return *this;
    }

    const Out& operator<<(const signed char* p) const
    {
        fprintf(file, "%p", p);
        return *this;
    }

    const Out& operator<<(const unsigned char* p) const
    {
        fprintf(file, "%p", p);
        return *this;
    }

    const Out& operator<<(const short* p) const
    {
        fprintf(file, "%p", p);
        return *this;
    }

    const Out& operator<<(const unsigned short* p) const
    {
        fprintf(file, "%p", p);
        return *this;
    }

    const Out& operator<<(const int* p) const
    {
        fprintf(file, "%p", p);
        return *this;
    }

    const Out& operator<<(const unsigned int* p) const
    {
        fprintf(file, "%p", p);
        return *this;
    }

    const Out& operator<<(const long* p) const
    {
        fprintf(file, "%p", p);
        return *this;
    }

    const Out& operator<<(const unsigned long* p) const
    {
        fprintf(file, "%p", p);
        return *this;
    }

    const Out& operator<<(const long long* p) const
    {
        fprintf(file, "%p", p);
        return *this;
    }

    const Out& operator<<(const unsigned long long* p) const
    {
        fprintf(file, "%p", p);
        return *this;
    }

    const Out& operator<<(const void* p) const
    {
        fprintf(file, "%p", p);
        return *this;
    }

};

}; // namespace OutStream
