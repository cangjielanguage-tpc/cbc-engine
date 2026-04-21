#include "utils/ostream.h"

#include "utils/assertion.h"

namespace Stream {

Out::Out(void* destStream) : dest(destStream) {}

void Out::NewLine() { PrintFmt("\n"); }

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

void Out::PrintFmt(const char* fmt, ...)
{
    BeforePrint();
    va_list args;
    va_start(args, fmt);
    PrintFmt(fmt, args);
    va_end(args);
    AfterPrint();
}

Out& Out::operator<<(const endl_t&)
{
    NewLine();
    return *this;
}


ToFile::ToFile(void* destStream) : Out(destStream) {}

const void* ToFile::Flush() const
{
    fflush((FILE*)dest);
    return dest;
}

void ToFile::BeforePrint() {}

void ToFile::AfterPrint() {}

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

const void* ToBuffer::Flush() const { return dest; }

void ToBuffer::BeforePrint() {}

void ToBuffer::AfterPrint() {}

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


ToIndentedBuffer::ToIndentedBuffer(void* destStream, const size_t bufSize, const unsigned int indentSize)
    : ToBuffer(destStream, bufSize),
      indentationSize(indentSize)
{}

void ToIndentedBuffer::BeforePrint()
{
    if (indentationSize) {
        BoundCheck(indentationSize);
        sprintf((char*)dest, "%*s", indentationSize, "");
        AdvanceDest(indentationSize - 1);
        indentationSize = 0;
    }
}


std::pair<std::shared_ptr<char[]>, std::shared_ptr<ToBuffer>> createBuffer(size_t bufSize)
{
    std::shared_ptr<char[]> buf(new char[bufSize]);
    std::shared_ptr<ToBuffer> stream = std::make_shared<ToBuffer>(buf.get(), bufSize);
    return std::make_pair(buf, stream);
}

std::pair<std::shared_ptr<char[]>, std::shared_ptr<ToIndentedBuffer>> createIndentedBuffer(size_t bufSize)
{
    std::shared_ptr<char[]> buf(new char[bufSize]);
    std::shared_ptr<ToIndentedBuffer> stream = std::make_shared<ToIndentedBuffer>(buf.get(), bufSize);
    return std::make_pair(buf, stream);
}

}; // namespace Stream
