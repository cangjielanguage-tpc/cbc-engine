#pragma once

#include <memory>
#include <stdarg.h>
#include <stdio.h>
#include <string>

namespace Stream {

struct endl_t {};

constexpr endl_t endl;

class Out {
public:
    Out(void* destStream);
    virtual ~Out() = default;

    virtual void Flush() const;
    virtual void NewLine();
    virtual void PrintFmt(const char* fmt, ...);
    virtual void PrintFmt(const char* fmt, va_list argp);

    Out& operator<<(const endl_t&);

    template <typename T> Out& operator<<(const T v)
    {
        Print(v);
        return *this;
    }

private:
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

protected:
    void* dest;

};

class ToFile : public Out {
public:
    ToFile(void* destStream);

    void Flush() const override;
    void PrintFmt(const char* fmt, ...) override;
    void PrintFmt(const char* fmt, va_list argp) override;
};

class ToBuffer : public Out {
public:
    size_t printed;
    size_t size;

    ToBuffer(void* destStream, size_t bufSize);

    void PrintFmt(const char* fmt, ...) override;
    void PrintFmt(const char* fmt, va_list argp) override;

protected:
    void BoundCheck(int requestedSize);
    void AdvanceDest(int printedSz);
};

class OutIndented : public Out {
public:
    Out& stream;
    unsigned int indentationSize;
    bool newLine = true;

    OutIndented(Out& astream, const unsigned int indentSize = 4);

    void NewLine() override;
    void PrintFmt(const char* fmt, ...) override;
    void PrintFmt(const char* fmt, va_list argp) override;
};

inline ToFile cout(stdout);
inline ToFile coutIndented(stdout);
inline ToFile cerr(stderr);
inline ToFile cerrIndented(stderr);

std::pair<std::shared_ptr<char[]>, std::shared_ptr<ToBuffer>> createBuffer(size_t bufSize);

}; // namespace Stream
