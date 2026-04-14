#include "utils/ostream.h"

#include "utils/assertion.h"

namespace Stream {

void OutputStrategy::NewLine(void* dest) { PrintFmt(dest, "\n"); }

void OutputStrategy::Print(void* dest, const float v) { PrintFmt(dest, "%f", v); }

void OutputStrategy::Print(void* dest, const double v) { PrintFmt(dest, "%f", v); }

void OutputStrategy::Print(void* dest, const long double v) { PrintFmt(dest, "%Lf", v); }

void OutputStrategy::Print(void* dest, const bool v) { PrintFmt(dest, "%s", v ? "true" : "false"); }

void OutputStrategy::Print(void* dest, const signed char v) { PrintFmt(dest, "%d", v); }

void OutputStrategy::Print(void* dest, const unsigned char v) { PrintFmt(dest, "%u", v); }

void OutputStrategy::Print(void* dest, const short v) { PrintFmt(dest, "%d", v); }

void OutputStrategy::Print(void* dest, const unsigned short v) { PrintFmt(dest, "%u", v); }

void OutputStrategy::Print(void* dest, const int v) { PrintFmt(dest, "%d", v); }

void OutputStrategy::Print(void* dest, const unsigned int v) { PrintFmt(dest, "%u", v); }

void OutputStrategy::Print(void* dest, const long v) { PrintFmt(dest, "%ld", v); }

void OutputStrategy::Print(void* dest, const unsigned long v) { PrintFmt(dest, "%ld", v); }

void OutputStrategy::Print(void* dest, const long long v) { PrintFmt(dest, "%lld", v); }

void OutputStrategy::Print(void* dest, const unsigned long long v) { PrintFmt(dest, "%llu", v); }

void OutputStrategy::Print(void* dest, const char c) { PrintFmt(dest, "%c", c); }

void OutputStrategy::Print(void* dest, const char* cstr) { PrintFmt(dest, "%s", cstr); }

void OutputStrategy::Print(void* dest, const std::string_view strv)
{
    PrintFmt(dest, "%.*s", static_cast<int>(strv.length()), strv.data());
}

void OutputStrategy::Print(void* dest, const std::string str) { PrintFmt(dest, "%s", str.c_str()); }

void OutputStrategy::Print(void* dest, const signed char* p) { PrintFmt(dest, "%p", p); }

void OutputStrategy::Print(void* dest, const unsigned char* p) { PrintFmt(dest, "%p", p); }

void OutputStrategy::Print(void* dest, const short* p) { PrintFmt(dest, "%p", p); }

void OutputStrategy::Print(void* dest, const unsigned short* p) { PrintFmt(dest, "%p", p); }

void OutputStrategy::Print(void* dest, const int* p) { PrintFmt(dest, "%p", p); }

void OutputStrategy::Print(void* dest, const unsigned int* p) { PrintFmt(dest, "%p", p); }

void OutputStrategy::Print(void* dest, const long* p) { PrintFmt(dest, "%p", p); }

void OutputStrategy::Print(void* dest, const unsigned long* p) { PrintFmt(dest, "%p", p); }

void OutputStrategy::Print(void* dest, const long long* p) { PrintFmt(dest, "%p", p); }

void OutputStrategy::Print(void* dest, const unsigned long long* p) { PrintFmt(dest, "%p", p); }

void OutputStrategy::Print(void* dest, const void* p) { PrintFmt(dest, "%p", p); }


const void* ToFile::Flush(void* file) const
{
    fflush((FILE*)file);
    return file;
}

void ToFile::BeforePrint(void* file) {}

void ToFile::AfterPrint(void* file) {}

void ToFile::PrintFmt(void* dest, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf((FILE*)dest, fmt, args);
    va_end(args);
}

void ToFile::PrintFmt(void* dest, const char* fmt, va_list argp) { vfprintf((FILE*)dest, fmt, argp); }


ToBuffer::ToBuffer(size_t bufSize) : OutputStrategy(), printed(0ull), size(bufSize) {}

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

const void* ToBuffer::Flush(void* dest) const { return dest; }

void ToBuffer::BeforePrint(void* file) {}

void ToBuffer::AfterPrint(void* file) {}

void ToBuffer::PrintFmt(void* dest, const char* fmt, ...)
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

void ToBuffer::PrintFmt(void* dest, const char* fmt, va_list argp)
{
    va_list copied;
    va_copy(copied, argp);
    int sz = vsnprintf(NULL, 0ull, fmt, copied) + 1;
    va_end(copied);
    BoundCheck(sz);
    size_t printedsz = vsnprintf((char*)dest + printed, sz, fmt, argp);
    AdvanceDest(printedsz);
}


ToIndentedBuffer::ToIndentedBuffer(const size_t bufSize, const unsigned int indentSize)
    : ToBuffer(bufSize),
      indentationSize(indentSize)
{}

void ToIndentedBuffer::BeforePrint(void* dest)
{
    if (indentationSize) {
        BoundCheck(indentationSize);
        sprintf((char*)dest, "%*s", indentationSize, "");
        AdvanceDest(indentationSize - 1);
        indentationSize = 0;
    }
}

Out::Out(void* destStream) : dest(destStream), outputStrategy(nullptr)
{
    static std::shared_ptr<OutputStrategy> toFile = std::make_shared<ToFile>();
    outputStrategy                                = toFile;
}

Out::Out(void* destStream, std::shared_ptr<OutputStrategy> strategy) : dest(destStream), outputStrategy(strategy) {}

const Out& Out::operator<<(const endl_t&) const
{
    outputStrategy->NewLine(dest);
    return *this;
}

const void Out::PrintFmt(const char* fmt, ...) const
{
    outputStrategy->BeforePrint(dest);
    va_list args;
    va_start(args, fmt);
    outputStrategy->PrintFmt(dest, fmt, args);
    va_end(args);
    outputStrategy->AfterPrint(dest);
}

const void* Out::Flush() const { return outputStrategy->Flush(dest); }

}; // namespace Stream
