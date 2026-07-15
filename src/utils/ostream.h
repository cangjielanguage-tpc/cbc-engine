#pragma once

#include <functional>
#include <memory>
#include <stdarg.h>
#include <stdio.h>
#include <string>
#include <string_view>

namespace Stream {

struct endl_t {};

constexpr endl_t endl;

class Output {
public:
    virtual ~Output() = default;

    Output& operator<<(const endl_t&);

    template <typename T> Output& operator<<(const T v)
    {
        Print(v);
        return *this;
    }

    virtual void Flush() const;
    virtual void NewLine();
    virtual void VPrintFmt(const char* fmt, va_list argp) = 0;

    void PrintFmt(const char* fmt, ...);
    void PrintFmtLn(const char* fmt, ...);

    void Print(const float v);
    void Print(const double v);
    void Print(const long double v);
    void Print(const bool v);
    void Print(const signed char v);
    void Print(const unsigned char v);
    void Print(const short v);
    void Print(const unsigned short v);
    void Print(const int v);
    void Print(const unsigned int v);
    void Print(const long v);
    void Print(const unsigned long v);
    void Print(const long long v);
    void Print(const unsigned long long v);
    void Print(const char c);
    void Print(const char* cstr);
    void Print(const std::string_view strv);
    void Print(const std::string str);
    void Print(const signed char* p);
    void Print(const unsigned char* p);
    void Print(const short* p);
    void Print(const unsigned short* p);
    void Print(const int* p);
    void Print(const unsigned int* p);
    void Print(const long* p);
    void Print(const unsigned long* p);
    void Print(const long long* p);
    void Print(const unsigned long long* p);
    void Print(const void* p);

    template <typename T1, typename T2> void Print(const std::pair<T1, T2> pair)
    {
        *this << "(" << pair.first << ", " << pair.second << ")";
    }
};

class FileOutput : public Output {
public:
    FileOutput(FILE* destStream);

    void Flush() const override;
    void VPrintFmt(const char* fmt, va_list argp) override;

protected:
    FILE* dest;
};

class StringBuffer : public Output {
public:
    StringBuffer();

    void VPrintFmt(const char* fmt, va_list argp) override;
    size_t Size() const;
    std::string ToString();
    char* ToCString();
    void Clear();

private:
    std::unique_ptr<char[]> data;
    size_t size;
    size_t capacity;
};

class Indented : public Output {
public:
    Indented(Output& astream, const unsigned int indentSize = 0);

    void NewLine() override;
    void VPrintFmt(const char* fmt, va_list argp) override;
    void Flush() const override;
    void SetIndent(unsigned int indent);
    unsigned int GetIndent() const;

private:
    Output& stream;
    unsigned int indentationSize;
    bool newLine = true;
};

class Descripted : public Output {
public:
    Descripted(Output& astream, std::string beforeDescription);

    void VPrintFmt(const char* fmt, va_list argp) override;
    void NewLine() override;
    void Flush() const override;

private:
    Output& stream;
    std::string beforeDesc;
    bool newLine = true;
};

extern FileOutput cout;
extern Output& cerr;

namespace Disasm {
extern Descripted isa;
extern Descripted rt;
} // namespace Disasm

}; // namespace Stream
