#pragma once

#include <stdio.h>
#include <string>
#include <memory>
#include <stdarg.h>

namespace Stream {

struct endl_t {};
constexpr endl_t endl;

class OutputStrategy {
public:
    virtual ~OutputStrategy() = default;

    virtual const void* Flush(void* dest) const = 0;
    virtual void NewLine(void* dest) = 0;
    virtual void BeforePrint(void* dest) = 0;
    virtual void AfterPrint(void* dest) = 0;
    virtual void Print(void* dest, const float v) = 0;
    virtual void Print(void* dest, const double v) = 0;
    virtual void Print(void* dest, const long double v) = 0;
    virtual void Print(void* dest, const bool v) = 0;
    virtual void Print(void* dest, const signed char v) = 0;
    virtual void Print(void* dest, const unsigned char v) = 0;
    virtual void Print(void* dest, const short v) = 0;
    virtual void Print(void* dest, const unsigned short v) = 0;
    virtual void Print(void* dest, const int v) = 0;
    virtual void Print(void* dest, const unsigned int v) = 0;
    virtual void Print(void* dest, const long v) = 0;
    virtual void Print(void* dest, const unsigned long v) = 0;
    virtual void Print(void* dest, const long long v) = 0;
    virtual void Print(void* dest, const unsigned long long v) = 0;
    virtual void Print(void* dest, const char c) = 0;
    virtual void Print(void* dest, const char* cstr) = 0;
    virtual void Print(void* dest, const std::string_view strv) = 0;
    virtual void Print(void* dest, const std::string str) = 0;
    virtual void Print(void* dest, const signed char* p) = 0;
    virtual void Print(void* dest, const unsigned char* p) = 0;
    virtual void Print(void* dest, const short* p) = 0;
    virtual void Print(void* dest, const unsigned short* p) = 0;
    virtual void Print(void* dest, const int* p) = 0;
    virtual void Print(void* dest, const unsigned int* p) = 0;
    virtual void Print(void* dest, const long* p) = 0;
    virtual void Print(void* dest, const unsigned long* p) = 0;
    virtual void Print(void* dest, const long long* p) = 0;
    virtual void Print(void* dest, const unsigned long long* p) = 0;
    virtual void Print(void* dest, const void* p) = 0;
    virtual void PrintFmt(void* dest, const char* fmt, va_list argp) = 0;
};

class ToFile : public OutputStrategy {
public:
    const void* Flush(void* dest) const override;
    void NewLine(void* dest) override;
    void BeforePrint(void* dest) override;
    void AfterPrint(void* dest) override;
    void Print(void* dest, const float v) override;
    void Print(void* dest, const double v) override;
    void Print(void* dest, const long double v) override;
    void Print(void* dest, const bool v) override;
    void Print(void* dest, const signed char v) override;
    void Print(void* dest, const unsigned char v) override;
    void Print(void* dest, const short v) override;
    void Print(void* dest, const unsigned short v) override;
    void Print(void* dest, const int v) override;
    void Print(void* dest, const unsigned int v) override;
    void Print(void* dest, const long v) override;
    void Print(void* dest, const unsigned long v) override;
    void Print(void* dest, const long long v) override;
    void Print(void* dest, const unsigned long long v) override;
    void Print(void* dest, const char c) override;
    void Print(void* dest, const char* cstr) override;
    void Print(void* dest, const std::string_view strv) override;
    void Print(void* dest, const std::string str) override;
    void Print(void* dest, const signed char* p) override;
    void Print(void* dest, const unsigned char* p) override;
    void Print(void* dest, const short* p) override;
    void Print(void* dest, const unsigned short* p) override;
    void Print(void* dest, const int* p) override;
    void Print(void* dest, const unsigned int* p) override;
    void Print(void* dest, const long* p) override;
    void Print(void* dest, const unsigned long* p) override;
    void Print(void* dest, const long long* p) override;
    void Print(void* dest, const unsigned long long* p) override;
    void Print(void* dest, const void* p) override;
    void PrintFmt(void* dest, const char* fmt, va_list argp) override;
};

class ToBuffer : public OutputStrategy {
public:
    size_t printed;
    size_t size;

    ToBuffer(size_t bufSize);

    const void* Flush(void* dest) const override;
    void NewLine(void* dest) override;
    void BeforePrint(void* dest) override;
    void AfterPrint(void* dest) override;
    void Print(void* dest, const float v) override;
    void Print(void* dest, const double v) override;
    void Print(void* dest, const long double v) override;
    void Print(void* dest, const bool v) override;
    void Print(void* dest, const signed char v) override;
    void Print(void* dest, const unsigned char v) override;
    void Print(void* dest, const short v) override;
    void Print(void* dest, const unsigned short v) override;
    void Print(void* dest, const int v) override;
    void Print(void* dest, const unsigned int v) override;
    void Print(void* dest, const long v) override;
    void Print(void* dest, const unsigned long v) override;
    void Print(void* dest, const long long v) override;
    void Print(void* dest, const unsigned long long v) override;
    void Print(void* dest, const char c) override;
    void Print(void* dest, const char* cstr) override;
    void Print(void* dest, const std::string_view strv) override;
    void Print(void* dest, const std::string str) override;
    void Print(void* dest, const signed char* p) override;
    void Print(void* dest, const unsigned char* p) override;
    void Print(void* dest, const short* p) override;
    void Print(void* dest, const unsigned short* p) override;
    void Print(void* dest, const int* p) override;
    void Print(void* dest, const unsigned int* p) override;
    void Print(void* dest, const long* p) override;
    void Print(void* dest, const unsigned long* p) override;
    void Print(void* dest, const long long* p) override;
    void Print(void* dest, const unsigned long long* p) override;
    void Print(void* dest, const void* p) override;
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

    template<typename T>
    const Out& operator<<(const T v) const
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

}; // namespace OutStream
