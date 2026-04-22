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
    virtual ~Out() = default;

    Out& operator<<(const endl_t&);

    template <typename T> Out& operator<<(const T v)
    {
        Print(v);
        return *this;
    }

    virtual void Flush() const;
    virtual void NewLine();
    virtual void PrintFmt(const char* fmt, ...) = 0;
    virtual void PrintFmt(const char* fmt, va_list argp) = 0;

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

};

class ToFile : public Out {
public:
    ToFile(void* destStream);

    void Flush() const override;
    void PrintFmt(const char* fmt, ...) override;
    void PrintFmt(const char* fmt, va_list argp) override;

protected:
    void* dest;

};

class ToBuffer : public Out {
public:
    ToBuffer(void* destStream, size_t bufSize);

    void PrintFmt(const char* fmt, ...) override;
    void PrintFmt(const char* fmt, va_list argp) override;

protected:
    void BoundCheck(int requestedSize);
    void AdvanceDest(int printedSz);

protected:
    void* dest;

public:
    size_t printed;
    size_t size;

};

inline ToFile cout(stdout);
inline ToFile cerr(stderr);

std::pair<std::shared_ptr<char[]>, std::shared_ptr<ToBuffer>> createBuffer(size_t bufSize);

// Decorators

class OutDecorated : public Out {
public:
    OutDecorated(Out& astream);

    virtual void BeforePrint() = 0;
    virtual void AfterPrint() = 0;

    void PrintFmt(const char* fmt, ...) override;
    void PrintFmt(const char* fmt, va_list argp) override;

public:
    Out& stream;

};

class OutIndented : public OutDecorated {
public:
    OutIndented(Out& astream, const unsigned int indentSize = 4);

    void NewLine() override;
    void BeforePrint() override;
    void AfterPrint() override;

public:
    unsigned int indentationSize;
    bool newLine = true;

};

typedef const char*(*DescFunc)();
constexpr DescFunc defaultDescFunc = [](){ return ""; };

class OutDescripted : public OutDecorated {
public:
    OutDescripted(Out& astream, DescFunc beforeDescription = defaultDescFunc, DescFunc afterDescription = defaultDescFunc);

    void NewLine() override;
    void BeforePrint() override;
    void AfterPrint() override;

public:
    DescFunc beforeDesc;
    DescFunc afterDesc;
    bool newLine = true;

};

inline OutIndented coutIndented(cout);
inline OutIndented cerrIndented(cerr);
inline Stream::OutDescripted coutDisasm(cout, [](){ return "[disasm] "; });
inline Stream::OutDescripted coutLog(cout, [](){ return "[log] "; });
inline Stream::OutDescripted cerrDisasm(cout, [](){ return "[disasm] "; });
inline Stream::OutDescripted cerrLog(cout, [](){ return "[log] "; });

}; // namespace Stream
