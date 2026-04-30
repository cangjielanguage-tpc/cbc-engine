#include "utils/ostream.h"

#include "utils/assertion.h"
#include <cstddef>
#include <cstdio>
#include <iostream>
#include <memory>
#include <utility>

namespace Stream {

FileOutput cout(stdout);
FileOutput cerr(stderr);
Descripted Disasm::isa(cerr, "[dis-isa] ");
Descripted Disasm::rt(cerr, "[dis-rt] ");

void Output::Flush() const {}

void Output::NewLine()
{
    Print("\n");
    Flush();
}

void Output::PrintFmt(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    VPrintFmt(fmt, args);
    va_end(args);
}

void Output::Print(const float v) { PrintFmt("%f", v); }

void Output::Print(const double v) { PrintFmt("%f", v); }

void Output::Print(const long double v) { PrintFmt("%Lf", v); }

void Output::Print(const bool v) { PrintFmt("%s", v ? "true" : "false"); }

void Output::Print(const signed char v) { PrintFmt("%d", v); }

void Output::Print(const unsigned char v) { PrintFmt("%u", v); }

void Output::Print(const short v) { PrintFmt("%d", v); }

void Output::Print(const unsigned short v) { PrintFmt("%u", v); }

void Output::Print(const int v) { PrintFmt("%d", v); }

void Output::Print(const unsigned int v) { PrintFmt("%u", v); }

void Output::Print(const long v) { PrintFmt("%ld", v); }

void Output::Print(const unsigned long v) { PrintFmt("%ld", v); }

void Output::Print(const long long v) { PrintFmt("%lld", v); }

void Output::Print(const unsigned long long v) { PrintFmt("%llu", v); }

void Output::Print(const char c) { PrintFmt("%c", c); }

void Output::Print(const char* cstr) { PrintFmt("%s", cstr); }

void Output::Print(const std::string_view strv) { PrintFmt("%.*s", static_cast<int>(strv.length()), strv.data()); }

void Output::Print(const std::string str) { PrintFmt("%s", str.c_str()); }

void Output::Print(const signed char* p) { PrintFmt("%p", p); }

void Output::Print(const unsigned char* p) { PrintFmt("%p", p); }

void Output::Print(const short* p) { PrintFmt("%p", p); }

void Output::Print(const unsigned short* p) { PrintFmt("%p", p); }

void Output::Print(const int* p) { PrintFmt("%p", p); }

void Output::Print(const unsigned int* p) { PrintFmt("%p", p); }

void Output::Print(const long* p) { PrintFmt("%p", p); }

void Output::Print(const unsigned long* p) { PrintFmt("%p", p); }

void Output::Print(const long long* p) { PrintFmt("%p", p); }

void Output::Print(const unsigned long long* p) { PrintFmt("%p", p); }

void Output::Print(const void* p) { PrintFmt("%p", p); }

Output& Output::operator<<(const endl_t&)
{
    NewLine();
    return *this;
}

FileOutput::FileOutput(FILE* destStream) : Output(), dest(destStream) {}

void FileOutput::Flush() const { fflush(dest); }

void FileOutput::VPrintFmt(const char* fmt, va_list argp) { vfprintf(dest, fmt, argp); }

StringBuffer::StringBuffer() : data(nullptr), size(0), capacity(0) {}

void StringBuffer::Clear() { size = 0; }

void StringBuffer::VPrintFmt(const char* fmt, va_list argp)
{
    static constexpr size_t INITIAL_CAPACITY = 128;

    va_list copy;
    va_copy(copy, argp);
    int required = vsnprintf(0, 0, fmt, copy);
    va_end(copy);
    if (required < 0) {
        // Silently drop the message.
        // TODO: error message?
        return;
    }

    if (required > capacity - size) {
        // extra zero should be counted
        auto newCapacity = std::max(INITIAL_CAPACITY, size + required + 1);
        newCapacity      = std::max(newCapacity, 2 * capacity);

        auto newMem = std::make_unique<char[]>(newCapacity);
        std::copy(data.get(), data.get() + capacity, newMem.get());
        capacity = newCapacity;
        data     = std::move(newMem);
    }

    auto begin  = data.get() + size;
    auto left   = capacity - size;
    int written = vsnprintf(begin, left, fmt, argp);
    ASSERT(written == required);
    size += written;
}

std::string StringBuffer::ToString() { return std::string(data.get(), size); }

// Decorators

Indented::Indented(Output& astream, const unsigned int indentSize) : stream(astream), indentationSize(indentSize) {}

void Indented::NewLine()
{
    stream.NewLine();
    newLine = true;
}

void Indented::Flush() const { stream.Flush(); }

void Indented::SetIndent(std::function<unsigned int(unsigned int)> f) { indentationSize = f(indentationSize); }

void Indented::VPrintFmt(const char* fmt, va_list argp)
{
    if (newLine) {
        stream.PrintFmt("%*s", indentationSize, "");
        newLine = false;
    }
    stream.VPrintFmt(fmt, argp);
}

Descripted::Descripted(Output& astream, std::string beforeDescription) : stream(astream), beforeDesc(beforeDescription)
{}

void Descripted::NewLine()
{
    stream.NewLine();
    newLine = true;
}

void Descripted::Flush() const { stream.Flush(); }

void Descripted::VPrintFmt(const char* fmt, va_list argp)
{
    if (newLine) {
        stream.PrintFmt("%s", beforeDesc.c_str());
        newLine = false;
    }
    stream.VPrintFmt(fmt, argp);
}

}; // namespace Stream
