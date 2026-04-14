#pragma once

#include <memory>
#include <stdarg.h>
#include <stdio.h>
#include <string>

namespace Stream {

struct endl_t {};

constexpr endl_t endl;

class OutputStrategy {
public:
    virtual ~OutputStrategy() = default;

    virtual const void* Flush(void* dest) const                      = 0;
    virtual void BeforePrint(void* dest)                             = 0;
    virtual void AfterPrint(void* dest)                              = 0;
    void NewLine(void* dest);
    void Print(void* dest, const float v);
    void Print(void* dest, const double v);
    void Print(void* dest, const long double v);
    void Print(void* dest, const bool v);
    void Print(void* dest, const signed char v);
    void Print(void* dest, const unsigned char v);
    void Print(void* dest, const short v);
    void Print(void* dest, const unsigned short v);
    void Print(void* dest, const int v);
    void Print(void* dest, const unsigned int v);
    void Print(void* dest, const long v);
    void Print(void* dest, const unsigned long v);
    void Print(void* dest, const long long v);
    void Print(void* dest, const unsigned long long v);
    void Print(void* dest, const char c);
    void Print(void* dest, const char* cstr);
    void Print(void* dest, const std::string_view strv);
    void Print(void* dest, const std::string str);
    void Print(void* dest, const signed char* p);
    void Print(void* dest, const unsigned char* p);
    void Print(void* dest, const short* p);
    void Print(void* dest, const unsigned short* p);
    void Print(void* dest, const int* p);
    void Print(void* dest, const unsigned int* p);
    void Print(void* dest, const long* p);
    void Print(void* dest, const unsigned long* p);
    void Print(void* dest, const long long* p);
    void Print(void* dest, const unsigned long long* p);
    void Print(void* dest, const void* p);
    virtual void PrintFmt(void* dest, const char* fmt, ...) = 0;
    virtual void PrintFmt(void* dest, const char* fmt, va_list argp) = 0;
};

class ToFile : public OutputStrategy {
public:
    const void* Flush(void* dest) const override;
    void BeforePrint(void* dest) override;
    void AfterPrint(void* dest) override;
    void PrintFmt(void* dest, const char* fmt, ...) override;
    void PrintFmt(void* dest, const char* fmt, va_list argp) override;
};

class ToBuffer : public OutputStrategy {
public:
    size_t printed;
    size_t size;

    ToBuffer(size_t bufSize);

    const void* Flush(void* dest) const override;
    void BeforePrint(void* dest) override;
    void AfterPrint(void* dest) override;
    void PrintFmt(void* dest, const char* fmt, ...) override;
    void PrintFmt(void* dest, const char* fmt, va_list argp) override;

protected:
    void BoundCheck(int requestedSize);
    void AdvanceDest(int printedSz);
};

class ToIndentedBuffer : public ToBuffer {
public:
    unsigned int indentationSize;

    ToIndentedBuffer(const size_t bufSize, const unsigned int indentSize = 4);

    void BeforePrint(void* dest) override;
};

class Out {
public:
    Out(void* destStream);
    Out(void* destStream, std::shared_ptr<OutputStrategy> strategy);

    const Out& operator<<(const endl_t&) const;

    template <typename T> const Out& operator<<(const T v) const
    {
        outputStrategy->BeforePrint(dest);
        outputStrategy->Print(dest, v);
        outputStrategy->AfterPrint(dest);
        return *this;
    }

    const void PrintFmt(const char* fmt, ...) const;
    const void* Flush() const;

private:
    void* dest;
    std::shared_ptr<OutputStrategy> outputStrategy;
};

}; // namespace Stream
