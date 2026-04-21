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
    OutputStrategy(void* destStream);
    virtual ~OutputStrategy() = default;

    virtual const void* Flush() const                      = 0;
    virtual void BeforePrint()                             = 0;
    virtual void AfterPrint()                              = 0;
    void NewLine();
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
    virtual void PrintFmt(const char* fmt, ...) = 0;
    virtual void PrintFmt(const char* fmt, va_list argp) = 0;

protected:
    void* dest;

};

class ToFile : public OutputStrategy {
public:
    ToFile(void* destStream);

    const void* Flush() const override;
    void BeforePrint() override;
    void AfterPrint() override;
    void PrintFmt(const char* fmt, ...) override;
    void PrintFmt(const char* fmt, va_list argp) override;
};

class ToBuffer : public OutputStrategy {
public:
    size_t printed;
    size_t size;

    ToBuffer(void* destStream, size_t bufSize);

    const void* Flush() const override;
    void BeforePrint() override;
    void AfterPrint() override;
    void PrintFmt(const char* fmt, ...) override;
    void PrintFmt(const char* fmt, va_list argp) override;

protected:
    void BoundCheck(int requestedSize);
    void AdvanceDest(int printedSz);
};

class ToIndentedBuffer : public ToBuffer {
public:
    unsigned int indentationSize;

    ToIndentedBuffer(void* destStream, const size_t bufSize, const unsigned int indentSize = 4);

    void BeforePrint() override;
};

class Out {
public:
    Out(void* destStream);
    Out(std::shared_ptr<OutputStrategy> strategy);

    const Out& operator<<(const endl_t&) const;

    template <typename T> const Out& operator<<(const T v) const
    {
        outputStrategy->BeforePrint();
        outputStrategy->Print(v);
        outputStrategy->AfterPrint();
        return *this;
    }

    const void PrintFmt(const char* fmt, ...) const;
    const void* Flush() const;

private:
    std::shared_ptr<OutputStrategy> outputStrategy;
};

std::pair<std::shared_ptr<char[]>, std::shared_ptr<ToBuffer>> createBuffer(size_t bufSize);
std::pair<std::shared_ptr<char[]>, std::shared_ptr<ToIndentedBuffer>> createIndentedBuffer(size_t bufSize);

}; // namespace Stream
