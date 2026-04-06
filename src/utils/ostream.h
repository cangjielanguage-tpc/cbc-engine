#pragma once

#include <stdio.h>
#include <string>
#include <memory>
#include <stdarg.h>
#include "utils/assertion.h"

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
    const void* Flush(void* file) const override {
        fflush((FILE*) file);
        return file;
    }

    void NewLine(void* file) override {
        fprintf((FILE*) file, "\n");
    }

    void BeforePrint(void* file) override {}
    void AfterPrint(void* file) override {}

    void Print(void* dest, const float v) override
    {
        fprintf((FILE*) dest, "%f", v);
    }

    void Print(void* dest, const double v) override
    {
        fprintf((FILE*) dest, "%f", v);
    }

    void Print(void* dest, const long double v) override
    {
        fprintf((FILE*) dest, "%Lf", v);
    }

    void Print(void* dest, const bool v) override
    {
        fprintf((FILE*) dest, "%s", v ? "true" : "false");
    }

    void Print(void* dest, const signed char v) override
    {
        fprintf((FILE*) dest, "%d", v);
    }

    void Print(void* dest, const unsigned char v) override
    {
        fprintf((FILE*) dest, "%u", v);
    }

    void Print(void* dest, const short v) override
    {
        fprintf((FILE*) dest, "%d", v);
    }

    void Print(void* dest, const unsigned short v) override
    {
        fprintf((FILE*) dest, "%u", v);
    }

    void Print(void* dest, const int v) override
    {
        fprintf((FILE*) dest, "%d", v);
    }

    void Print(void* dest, const unsigned int v) override
    {
        fprintf((FILE*) dest, "%u", v);
    }

    void Print(void* dest, const long v) override
    {
        fprintf((FILE*) dest, "%ld", v);
    }

    void Print(void* dest, const unsigned long v) override
    {
        fprintf((FILE*) dest, "%ld", v);
    }

    void Print(void* dest, const long long v) override
    {
        fprintf((FILE*) dest, "%lld", v);
    }

    void Print(void* dest, const unsigned long long v) override
    {
        fprintf((FILE*) dest, "%llu", v);
    }

    void Print(void* dest, const char c) override
    {
        fprintf((FILE*) dest, "%c", c);
    }

    void Print(void* dest, const char* cstr) override
    {
        fprintf((FILE*) dest, "%s", cstr);
    }

    void Print(void* dest, const std::string_view strv) override
    {
        fprintf((FILE*) dest, "%.*s", static_cast<int>(strv.length()), strv.data());
    }

    void Print(void* dest, const std::string str) override
    {
        fprintf((FILE*) dest, "%s", str.c_str());
    }

    void Print(void* dest, const signed char* p) override
    {
        fprintf((FILE*) dest, "%p", p);
    }

    void Print(void* dest, const unsigned char* p) override
    {
        fprintf((FILE*) dest, "%p", p);
    }

    void Print(void* dest, const short* p) override
    {
        fprintf((FILE*) dest, "%p", p);
    }

    void Print(void* dest, const unsigned short* p) override
    {
        fprintf((FILE*) dest, "%p", p);
    }

    void Print(void* dest, const int* p) override
    {
        fprintf((FILE*) dest, "%p", p);
    }

    void Print(void* dest, const unsigned int* p) override
    {
        fprintf((FILE*) dest, "%p", p);
    }

    void Print(void* dest, const long* p) override
    {
        fprintf((FILE*) dest, "%p", p);
    }

    void Print(void* dest, const unsigned long* p) override
    {
        fprintf((FILE*) dest, "%p", p);
    }

    void Print(void* dest, const long long* p) override
    {
        fprintf((FILE*) dest, "%p", p);
    }

    void Print(void* dest, const unsigned long long* p) override
    {
        fprintf((FILE*) dest, "%p", p);
    }

    void Print(void* dest, const void* p) override
    {
        fprintf((FILE*) dest, "%p", p);
    }

    void PrintFmt(void* dest, const char* fmt, va_list argp) override
    {
        vfprintf((FILE*) dest, fmt, argp);
    }
};

class ToBuffer : public OutputStrategy {
public:
    size_t printed;
    size_t size;

    ToBuffer(size_t bufSize) : OutputStrategy(), printed(0ull), size(bufSize) {}

    void BoundCheck(int requestedSize)
    {
        ASSERTION(requestedSize >= 0, "error during printing length calculation");
        ASSERTION(printed + requestedSize <= size, "buffer size limit exceeded");
    }

    void AdvanceDest(int printedSz)
    {
        ASSERTION(printedSz >= 0, "error during printing");
        printed += printedSz;
    }

    const void* Flush(void* dest) const override
    {
        return dest;
    }

    void NewLine(void* dest) override {
        int sz = 2ull;
        BoundCheck(sz);
        size_t printedSz = snprintf((char*) dest + printed, sz, "\n");
        AdvanceDest(printedSz);
    }

    void BeforePrint(void* file) override {}
    void AfterPrint(void* file) override {}

    void Print(void* dest, const float v) override
    {
        int sz = snprintf(NULL, 0ull, "%f", v) + 1;
        BoundCheck(sz);
        size_t printedSz = snprintf((char*) dest + printed, sz, "%f", v);
        AdvanceDest(printedSz);
    }

    void Print(void* dest, const double v) override
    {
        int sz = snprintf(NULL, 0ull, "%f", v) + 1;
        BoundCheck(sz);
        size_t printedSz = snprintf((char*) dest + printed, sz, "%f", v);
        AdvanceDest(printedSz);
    }

    void Print(void* dest, const long double v) override
    {
        int sz = snprintf(NULL, 0ull, "%Lf", v) + 1;
        BoundCheck(sz);
        size_t printedSz = snprintf((char*) dest + printed, sz, "%Lf", v);
        AdvanceDest(printedSz);
    }

    void Print(void* dest, const bool v) override
    {
        int sz = snprintf(NULL, 0ull, "%s", v ? "true" : "false") + 1;
        BoundCheck(sz);
        size_t printedSz = snprintf((char*) dest + printed, sz, "%s", v ? "true" : "false");
        AdvanceDest(printedSz);
    }

    void Print(void* dest, const signed char v) override
    {
        int sz = snprintf(NULL, 0ull, "%d", v) + 1;
        BoundCheck(sz);
        size_t printedSz = snprintf((char*) dest + printed, sz, "%d", v);
        AdvanceDest(printedSz);
    }

    void Print(void* dest, const unsigned char v) override
    {
        int sz = snprintf(NULL, 0ull, "%u", v) + 1;
        BoundCheck(sz);
        size_t printedSz = snprintf((char*) dest + printed, sz, "%u", v);
        AdvanceDest(printedSz);
    }

    void Print(void* dest, const short v) override
    {
        int sz = snprintf(NULL, 0ull, "%d", v) + 1;
        BoundCheck(sz);
        size_t printedSz = snprintf((char*) dest + printed, sz, "%d", v);
        AdvanceDest(printedSz);
    }

    void Print(void* dest, const unsigned short v) override
    {
        int sz = snprintf(NULL, 0ull, "%u", v) + 1;
        BoundCheck(sz);
        size_t printedSz = snprintf((char*) dest + printed, sz, "%u", v);
        AdvanceDest(printedSz);
    }

    void Print(void* dest, const int v) override
    {
        int sz = snprintf(NULL, 0ull, "%d", v) + 1;
        BoundCheck(sz);
        size_t printedSz = snprintf((char*) dest + printed, sz, "%d", v);
        AdvanceDest(printedSz);
    }

    void Print(void* dest, const unsigned int v) override
    {
        int sz = snprintf(NULL, 0ull, "%u", v) + 1;
        BoundCheck(sz);
        size_t printedSz = snprintf((char*) dest + printed, sz, "%u", v);
        AdvanceDest(printedSz);
    }

    void Print(void* dest, const long v) override
    {
        int sz = snprintf(NULL, 0ull, "%ld", v) + 1;
        BoundCheck(sz);
        size_t printedSz = snprintf((char*) dest + printed, sz, "%ld", v);
        AdvanceDest(printedSz);
    }

    void Print(void* dest, const unsigned long v) override
    {
        int sz = snprintf(NULL, 0ull, "%ld", v) + 1;
        BoundCheck(sz);
        size_t printedSz = snprintf((char*) dest + printed, sz, "%ld", v);
        AdvanceDest(printedSz);
    }

    void Print(void* dest, const long long v) override
    {
        int sz = snprintf(NULL, 0ull, "%lld", v) + 1;
        BoundCheck(sz);
        size_t printedSz = snprintf((char*) dest + printed, sz, "%lld", v);
        AdvanceDest(printedSz);
    }

    void Print(void* dest, const unsigned long long v) override
    {
        int sz = snprintf(NULL, 0ull, "%llu", v) + 1;
        BoundCheck(sz);
        size_t printedSz = snprintf((char*) dest + printed, sz, "%llu", v);
        AdvanceDest(printedSz);
    }

    void Print(void* dest, const char c) override
    {
        int sz = snprintf(NULL, 0ull, "%c", c) + 1;
        BoundCheck(sz);
        size_t printedSz = snprintf((char*) dest + printed, sz, "%c", c);
        AdvanceDest(printedSz);
    }

    void Print(void* dest, const char* cstr) override
    {
        int sz = snprintf(NULL, 0ull, "%s", cstr) + 1;
        BoundCheck(sz);
        size_t printedSz = snprintf((char*) dest + printed, sz, "%s", cstr);
        AdvanceDest(printedSz);
    }

    void Print(void* dest, const std::string_view strv) override
    {
        int sz = snprintf(NULL, 0ull,  "%.*s", static_cast<int>(strv.length()), strv.data()) + 1;
        BoundCheck(sz);
        size_t printedSz = snprintf((char*) dest + printed, sz, "%.*s", static_cast<int>(strv.length()), strv.data());
        AdvanceDest(printedSz);
    }

    void Print(void* dest, const std::string str) override
    {
        int sz = snprintf(NULL, 0ull, "%s", str.c_str()) + 1;
        BoundCheck(sz);
        size_t printedSz = snprintf((char*) dest + printed, sz, "%s", str.c_str());
        AdvanceDest(printedSz);
    }

    void Print(void* dest, const signed char* p) override
    {
        int sz = snprintf(NULL, 0ull, "%p", p) + 1;
        BoundCheck(sz);
        size_t printedSz = snprintf((char*) dest + printed, sz, "%p", p);
        AdvanceDest(printedSz);
    }

    void Print(void* dest, const unsigned char* p) override
    {
        int sz = snprintf(NULL, 0ull, "%p", p) + 1;
        BoundCheck(sz);
        size_t printedSz = snprintf((char*) dest + printed, sz, "%p", p);
        AdvanceDest(printedSz);
    }

    void Print(void* dest, const short* p) override
    {
        int sz = snprintf(NULL, 0ull, "%p", p) + 1;
        BoundCheck(sz);
        size_t printedSz = snprintf((char*) dest + printed, sz, "%p", p);
        AdvanceDest(printedSz);
    }

    void Print(void* dest, const unsigned short* p) override
    {
        int sz = snprintf(NULL, 0ull, "%p", p) + 1;
        BoundCheck(sz);
        size_t printedSz = snprintf((char*) dest + printed, sz, "%p", p);
        AdvanceDest(printedSz);
    }

    void Print(void* dest, const int* p) override
    {
        int sz = snprintf(NULL, 0ull, "%p", p) + 1;
        BoundCheck(sz);
        size_t printedSz = snprintf((char*) dest + printed, sz, "%p", p);
        AdvanceDest(printedSz);
    }

    void Print(void* dest, const unsigned int* p) override
    {
        int sz = snprintf(NULL, 0ull, "%p", p);
        BoundCheck(sz);
        size_t printedSz = snprintf((char*) dest + printed, sz, "%p", p);
        AdvanceDest(printedSz);
    }

    void Print(void* dest, const long* p) override
    {
        int sz = snprintf(NULL, 0ull, "%p", p);
        BoundCheck(sz);
        size_t printedSz = snprintf((char*) dest + printed, sz, "%p", p);
        AdvanceDest(printedSz);
    }

    void Print(void* dest, const unsigned long* p) override
    {
        int sz = snprintf(NULL, 0ull, "%p", p) + 1;
        BoundCheck(sz);
        size_t printedSz = snprintf((char*) dest + printed, sz, "%p", p);
        AdvanceDest(printedSz);
    }

    void Print(void* dest, const long long* p) override
    {
        int sz = snprintf(NULL, 0ull, "%p", p) + 1;
        BoundCheck(sz);
        size_t printedSz = snprintf((char*) dest + printed, sz, "%p", p);
        AdvanceDest(printedSz);
    }

    void Print(void* dest, const unsigned long long* p) override
    {
        int sz = snprintf(NULL, 0ull, "%p", p) + 1;
        BoundCheck(sz);
        size_t printedSz = snprintf((char*) dest + printed, sz, "%p", p);
        AdvanceDest(printedSz);
    }

    void Print(void* dest, const void* p) override
    {
        int sz = snprintf(NULL, 0ull, "%p", p) + 1;
        BoundCheck(sz);
        size_t printedSz = snprintf((char*) dest + printed, sz, "%p", p);
        AdvanceDest(printedSz);
    }

    void PrintFmt(void* dest, const char* fmt, va_list argp) override
    {
        va_list copied;
        va_copy(copied, argp);
        int sz = vsnprintf(NULL, 0ull, fmt, copied) + 1;
        BoundCheck(sz);
        size_t printedSz = vsnprintf((char*) dest + printed, sz, fmt, argp);
        AdvanceDest(printedSz);
    }
};

class ToIndentedBuffer : public ToBuffer {
public:
    unsigned int indentationSize;

    ToIndentedBuffer(const size_t bufSize, const unsigned int indentSize = 4) : ToBuffer(bufSize), indentationSize(indentSize) {}

    void BeforePrint(void* dest) override
    {
        if (indentationSize) {
            BoundCheck(indentationSize);
            sprintf((char*) dest, "%*s", indentationSize, "");
            AdvanceDest(indentationSize - 1);
            indentationSize = 0;
        }
    }

};

class Out {
public:
    Out(void* destStream) : dest(destStream), outputStrategy(nullptr) {
        static std::shared_ptr<OutputStrategy> toFile = std::make_shared<ToFile>();
        outputStrategy = toFile;
    }

    // owning outputStrategy
    Out(void* destStream, std::shared_ptr<OutputStrategy> strategy) : dest(destStream), outputStrategy(strategy) {}

    const Out& operator<<(const endl_t&) const
    {
        outputStrategy->NewLine(dest);
        return *this;
    }

    template<typename T>
    const Out& operator<<(const T v) const
    {
        outputStrategy->BeforePrint(dest);
        outputStrategy->Print(dest, v);
        outputStrategy->AfterPrint(dest);
        return *this;
    }

    const void PrintFmt(const char* fmt, ...) const
    {
        outputStrategy->BeforePrint(dest);
        va_list args;
        va_start(args, fmt);
        outputStrategy->PrintFmt(dest, fmt, args);
        va_end(args);
        outputStrategy->AfterPrint(dest);
    }

    const void* Flush() const
    {
        return outputStrategy->Flush(dest);
    }

private:
    void* dest;
    std::shared_ptr<OutputStrategy> outputStrategy;

};

}; // namespace OutStream
