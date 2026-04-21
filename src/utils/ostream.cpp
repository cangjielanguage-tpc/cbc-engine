#include "utils/ostream.h"

#include "utils/assertion.h"

namespace Stream {

Out::Out(void* destStream) : dest(destStream) {}

void Out::Flush() const {}

void Out::NewLine() { PrintFmt("\n"); }

void Out::PrintFmt(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    PrintFmt(fmt, args);
    va_end(args);
}

void Out::PrintFmt(const char* fmt, va_list argp) { vfprintf((FILE*)dest, fmt, argp); }

void Out::Print(const float v) { PrintFmt("%f", v); }

void Out::Print(const double v) { PrintFmt("%f", v); }

void Out::Print(const long double v) { PrintFmt("%Lf", v); }

void Out::Print(const bool v) { PrintFmt("%s", v ? "true" : "false"); }

void Out::Print(const signed char v) { PrintFmt("%d", v); }

void Out::Print(const unsigned char v) { PrintFmt("%u", v); }

void Out::Print(const short v) { PrintFmt("%d", v); }

void Out::Print(const unsigned short v) { PrintFmt("%u", v); }

void Out::Print(const int v) { PrintFmt("%d", v); }

void Out::Print(const unsigned int v) { PrintFmt("%u", v); }

void Out::Print(const long v) { PrintFmt("%ld", v); }

void Out::Print(const unsigned long v) { PrintFmt("%ld", v); }

void Out::Print(const long long v) { PrintFmt("%lld", v); }

void Out::Print(const unsigned long long v) { PrintFmt("%llu", v); }

void Out::Print(const char c) { PrintFmt("%c", c); }

void Out::Print(const char* cstr) { PrintFmt("%s", cstr); }

void Out::Print(const std::string_view strv)
{
    PrintFmt("%.*s", static_cast<int>(strv.length()), strv.data());
}

void Out::Print(const std::string str) { PrintFmt("%s", str.c_str()); }

void Out::Print(const signed char* p) { PrintFmt("%p", p); }

void Out::Print(const unsigned char* p) { PrintFmt("%p", p); }

void Out::Print(const short* p) { PrintFmt("%p", p); }

void Out::Print(const unsigned short* p) { PrintFmt("%p", p); }

void Out::Print(const int* p) { PrintFmt("%p", p); }

void Out::Print(const unsigned int* p) { PrintFmt("%p", p); }

void Out::Print(const long* p) { PrintFmt("%p", p); }

void Out::Print(const unsigned long* p) { PrintFmt("%p", p); }

void Out::Print(const long long* p) { PrintFmt("%p", p); }

void Out::Print(const unsigned long long* p) { PrintFmt("%p", p); }

void Out::Print(const void* p) { PrintFmt("%p", p); }

Out& Out::operator<<(const endl_t&)
{
    NewLine();
    return *this;
}


ToFile::ToFile(void* destStream) : Out(destStream) {}

void ToFile::Flush() const
{
    fflush((FILE*)dest);
}

void ToFile::PrintFmt(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf((FILE*)dest, fmt, args);
    va_end(args);
}

void ToFile::PrintFmt(const char* fmt, va_list argp) { vfprintf((FILE*)dest, fmt, argp); }


ToBuffer::ToBuffer(void* destStream, size_t bufSize) : Out(destStream), printed(0ull), size(bufSize) {}

void ToBuffer::BoundCheck(int requestedSize)
{
    ASSERTION(requestedSize >= 0, "error during printing length calculation");
    ASSERTION(printed + requestedSize <= size, "buffer size limit exceeded");
}

void ToBuffer::AdvanceDest(int printedSz)
{
    ASSERTION(printedSz >= 0, "error during printing");
    printed += printedSz;
}

void ToBuffer::PrintFmt(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    va_list copied;
    va_copy(copied, args);
    int sz = vsnprintf(NULL, 0ull, fmt, copied) + 1;
    va_end(copied);
    BoundCheck(sz);
    size_t printedsz = vsnprintf((char*)dest + printed, sz, fmt, args);
    va_end(args);
    AdvanceDest(printedsz);
}

void ToBuffer::PrintFmt(const char* fmt, va_list argp)
{
    va_list copied;
    va_copy(copied, argp);
    int sz = vsnprintf(NULL, 0ull, fmt, copied) + 1;
    va_end(copied);
    BoundCheck(sz);
    size_t printedsz = vsnprintf((char*)dest + printed, sz, fmt, argp);
    AdvanceDest(printedsz);
}


OutIndented::OutIndented(Out& astream, const unsigned int indentSize)
    : Out(astream),
      stream(astream),
      indentationSize(indentSize)
{}

void OutIndented::NewLine()
{
    stream.NewLine();
    newLine = true;
}

void OutIndented::PrintFmt(const char* fmt, ...)
{
    if (newLine) {
        stream.PrintFmt("%*s", indentationSize, "");
        va_list args;
        va_start(args, fmt);
        stream.PrintFmt(fmt, args);
        va_end(args);
        newLine = false;
    }
}

void OutIndented::PrintFmt(const char* fmt, va_list argp)
{
    if (newLine) {
        stream.PrintFmt("%*s", indentationSize, "");
        stream.PrintFmt(fmt, argp);
        newLine = false;
    }
}

std::pair<std::shared_ptr<char[]>, std::shared_ptr<ToBuffer>> createBuffer(size_t bufSize)
{
    std::shared_ptr<char[]> buf(new char[bufSize]);
    std::shared_ptr<ToBuffer> stream(new ToBuffer(buf.get(), bufSize));
    return {buf, stream };
}

}; // namespace Stream
