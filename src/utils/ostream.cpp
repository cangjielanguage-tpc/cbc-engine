#include "utils/ostream.h"

#include "utils/assertion.h"

namespace Stream {

const void* ToFile::Flush(void* file) const
{
    fflush((FILE*) file);
    return file;
}

void ToFile::NewLine(void* file)
{
    fprintf((FILE*) file, "\n");
}

void ToFile::BeforePrint(void* file) {}
void ToFile::AfterPrint(void* file) {}

void ToFile::Print(void* dest, const float v)
{
    fprintf((FILE*) dest, "%f", v);
}

void ToFile::Print(void* dest, const double v)
{
    fprintf((FILE*) dest, "%f", v);
}

void ToFile::Print(void* dest, const long double v)
{
    fprintf((FILE*) dest, "%Lf", v);
}

void ToFile::Print(void* dest, const bool v)
{
    fprintf((FILE*) dest, "%s", v ? "true" : "false");
}

void ToFile::Print(void* dest, const signed char v)
{
    fprintf((FILE*) dest, "%d", v);
}

void ToFile::Print(void* dest, const unsigned char v)
{
    fprintf((FILE*) dest, "%u", v);
}

void ToFile::Print(void* dest, const short v)
{
    fprintf((FILE*) dest, "%d", v);
}

void ToFile::Print(void* dest, const unsigned short v)
{
    fprintf((FILE*) dest, "%u", v);
}

void ToFile::Print(void* dest, const int v)
{
    fprintf((FILE*) dest, "%d", v);
}

void ToFile::Print(void* dest, const unsigned int v)
{
    fprintf((FILE*) dest, "%u", v);
}

void ToFile::Print(void* dest, const long v)
{
    fprintf((FILE*) dest, "%ld", v);
}

void ToFile::Print(void* dest, const unsigned long v)
{
    fprintf((FILE*) dest, "%ld", v);
}

void ToFile::Print(void* dest, const long long v)
{
    fprintf((FILE*) dest, "%lld", v);
}

void ToFile::Print(void* dest, const unsigned long long v)
{
    fprintf((FILE*) dest, "%llu", v);
}

void ToFile::Print(void* dest, const char c)
{
    fprintf((FILE*) dest, "%c", c);
}

void ToFile::Print(void* dest, const char* cstr)
{
    fprintf((FILE*) dest, "%s", cstr);
}

void ToFile::Print(void* dest, const std::string_view strv)
{
    fprintf((FILE*) dest, "%.*s", static_cast<int>(strv.length()), strv.data());
}

void ToFile::Print(void* dest, const std::string str)
{
    fprintf((FILE*) dest, "%s", str.c_str());
}

void ToFile::Print(void* dest, const signed char* p)
{
    fprintf((FILE*) dest, "%p", p);
}

void ToFile::Print(void* dest, const unsigned char* p)
{
    fprintf((FILE*) dest, "%p", p);
}

void ToFile::Print(void* dest, const short* p)
{
    fprintf((FILE*) dest, "%p", p);
}

void ToFile::Print(void* dest, const unsigned short* p)
{
    fprintf((FILE*) dest, "%p", p);
}

void ToFile::Print(void* dest, const int* p)
{
    fprintf((FILE*) dest, "%p", p);
}

void ToFile::Print(void* dest, const unsigned int* p)
{
    fprintf((FILE*) dest, "%p", p);
}

void ToFile::Print(void* dest, const long* p)
{
    fprintf((FILE*) dest, "%p", p);
}

void ToFile::Print(void* dest, const unsigned long* p)
{
    fprintf((FILE*) dest, "%p", p);
}

void ToFile::Print(void* dest, const long long* p)
{
    fprintf((FILE*) dest, "%p", p);
}

void ToFile::Print(void* dest, const unsigned long long* p)
{
    fprintf((FILE*) dest, "%p", p);
}

void ToFile::Print(void* dest, const void* p)
{
    fprintf((FILE*) dest, "%p", p);
}

void ToFile::PrintFmt(void* dest, const char* fmt, va_list argp)
{
    vfprintf((FILE*) dest, fmt, argp);
}

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

const void* ToBuffer::Flush(void* dest) const
{
    return dest;
}

void ToBuffer::NewLine(void* dest) {
    int sz = 2ull;
    BoundCheck(sz);
    size_t printedSz = snprintf((char*) dest + printed, sz, "\n");
    AdvanceDest(printedSz);
}

void ToBuffer::BeforePrint(void* file) {}
void ToBuffer::AfterPrint(void* file) {}

void ToBuffer::Print(void* dest, const float v)
{
    int sz = snprintf(NULL, 0ull, "%f", v) + 1;
    BoundCheck(sz);
    size_t printedSz = snprintf((char*) dest + printed, sz, "%f", v);
    AdvanceDest(printedSz);
}

void ToBuffer::Print(void* dest, const double v)
{
    int sz = snprintf(NULL, 0ull, "%f", v) + 1;
    BoundCheck(sz);
    size_t printedSz = snprintf((char*) dest + printed, sz, "%f", v);
    AdvanceDest(printedSz);
}

void ToBuffer::Print(void* dest, const long double v)
{
    int sz = snprintf(NULL, 0ull, "%Lf", v) + 1;
    BoundCheck(sz);
    size_t printedSz = snprintf((char*) dest + printed, sz, "%Lf", v);
    AdvanceDest(printedSz);
}

void ToBuffer::Print(void* dest, const bool v)
{
    int sz = snprintf(NULL, 0ull, "%s", v ? "true" : "false") + 1;
    BoundCheck(sz);
    size_t printedSz = snprintf((char*) dest + printed, sz, "%s", v ? "true" : "false");
    AdvanceDest(printedSz);
}

void ToBuffer::Print(void* dest, const signed char v)
{
    int sz = snprintf(NULL, 0ull, "%d", v) + 1;
    BoundCheck(sz);
    size_t printedSz = snprintf((char*) dest + printed, sz, "%d", v);
    AdvanceDest(printedSz);
}

void ToBuffer::Print(void* dest, const unsigned char v)
{
    int sz = snprintf(NULL, 0ull, "%u", v) + 1;
    BoundCheck(sz);
    size_t printedSz = snprintf((char*) dest + printed, sz, "%u", v);
    AdvanceDest(printedSz);
}

void ToBuffer::Print(void* dest, const short v)
{
    int sz = snprintf(NULL, 0ull, "%d", v) + 1;
    BoundCheck(sz);
    size_t printedSz = snprintf((char*) dest + printed, sz, "%d", v);
    AdvanceDest(printedSz);
}

void ToBuffer::Print(void* dest, const unsigned short v)
{
    int sz = snprintf(NULL, 0ull, "%u", v) + 1;
    BoundCheck(sz);
    size_t printedSz = snprintf((char*) dest + printed, sz, "%u", v);
    AdvanceDest(printedSz);
}

void ToBuffer::Print(void* dest, const int v)
{
    int sz = snprintf(NULL, 0ull, "%d", v) + 1;
    BoundCheck(sz);
    size_t printedSz = snprintf((char*) dest + printed, sz, "%d", v);
    AdvanceDest(printedSz);
}

void ToBuffer::Print(void* dest, const unsigned int v)
{
    int sz = snprintf(NULL, 0ull, "%u", v) + 1;
    BoundCheck(sz);
    size_t printedSz = snprintf((char*) dest + printed, sz, "%u", v);
    AdvanceDest(printedSz);
}

void ToBuffer::Print(void* dest, const long v)
{
    int sz = snprintf(NULL, 0ull, "%ld", v) + 1;
    BoundCheck(sz);
    size_t printedSz = snprintf((char*) dest + printed, sz, "%ld", v);
    AdvanceDest(printedSz);
}

void ToBuffer::Print(void* dest, const unsigned long v)
{
    int sz = snprintf(NULL, 0ull, "%ld", v) + 1;
    BoundCheck(sz);
    size_t printedSz = snprintf((char*) dest + printed, sz, "%ld", v);
    AdvanceDest(printedSz);
}

void ToBuffer::Print(void* dest, const long long v)
{
    int sz = snprintf(NULL, 0ull, "%lld", v) + 1;
    BoundCheck(sz);
    size_t printedSz = snprintf((char*) dest + printed, sz, "%lld", v);
    AdvanceDest(printedSz);
}

void ToBuffer::Print(void* dest, const unsigned long long v)
{
    int sz = snprintf(NULL, 0ull, "%llu", v) + 1;
    BoundCheck(sz);
    size_t printedSz = snprintf((char*) dest + printed, sz, "%llu", v);
    AdvanceDest(printedSz);
}

void ToBuffer::Print(void* dest, const char c)
{
    int sz = snprintf(NULL, 0ull, "%c", c) + 1;
    BoundCheck(sz);
    size_t printedSz = snprintf((char*) dest + printed, sz, "%c", c);
    AdvanceDest(printedSz);
}

void ToBuffer::Print(void* dest, const char* cstr)
{
    int sz = snprintf(NULL, 0ull, "%s", cstr) + 1;
    BoundCheck(sz);
    size_t printedSz = snprintf((char*) dest + printed, sz, "%s", cstr);
    AdvanceDest(printedSz);
}

void ToBuffer::Print(void* dest, const std::string_view strv)
{
    int sz = snprintf(NULL, 0ull,  "%.*s", static_cast<int>(strv.length()), strv.data()) + 1;
    BoundCheck(sz);
    size_t printedSz = snprintf((char*) dest + printed, sz, "%.*s", static_cast<int>(strv.length()), strv.data());
    AdvanceDest(printedSz);
}

void ToBuffer::Print(void* dest, const std::string str)
{
    int sz = snprintf(NULL, 0ull, "%s", str.c_str()) + 1;
    BoundCheck(sz);
    size_t printedSz = snprintf((char*) dest + printed, sz, "%s", str.c_str());
    AdvanceDest(printedSz);
}

void ToBuffer::Print(void* dest, const signed char* p)
{
    int sz = snprintf(NULL, 0ull, "%p", p) + 1;
    BoundCheck(sz);
    size_t printedSz = snprintf((char*) dest + printed, sz, "%p", p);
    AdvanceDest(printedSz);
}

void ToBuffer::Print(void* dest, const unsigned char* p)
{
    int sz = snprintf(NULL, 0ull, "%p", p) + 1;
    BoundCheck(sz);
    size_t printedSz = snprintf((char*) dest + printed, sz, "%p", p);
    AdvanceDest(printedSz);
}

void ToBuffer::Print(void* dest, const short* p)
{
    int sz = snprintf(NULL, 0ull, "%p", p) + 1;
    BoundCheck(sz);
    size_t printedSz = snprintf((char*) dest + printed, sz, "%p", p);
    AdvanceDest(printedSz);
}

void ToBuffer::Print(void* dest, const unsigned short* p)
{
    int sz = snprintf(NULL, 0ull, "%p", p) + 1;
    BoundCheck(sz);
    size_t printedsz = snprintf((char*) dest + printed, sz, "%p", p);
    AdvanceDest(printedsz);
}

void ToBuffer::Print(void* dest, const int* p)
{
    int sz = snprintf(NULL, 0ull, "%p", p) + 1;
    BoundCheck(sz);
    size_t printedsz = snprintf((char*) dest + printed, sz, "%p", p);
    AdvanceDest(printedsz);
}

void ToBuffer::Print(void* dest, const unsigned int* p)
{
    int sz = snprintf(NULL, 0ull, "%p", p);
    BoundCheck(sz);
    size_t printedsz = snprintf((char*) dest + printed, sz, "%p", p);
    AdvanceDest(printedsz);
}

void ToBuffer::Print(void* dest, const long* p)
{
    int sz = snprintf(NULL, 0ull, "%p", p);
    BoundCheck(sz);
    size_t printedsz = snprintf((char*) dest + printed, sz, "%p", p);
    AdvanceDest(printedsz);
}

void ToBuffer::Print(void* dest, const unsigned long* p)
{
    int sz = snprintf(NULL, 0ull, "%p", p) + 1;
    BoundCheck(sz);
    size_t printedsz = snprintf((char*) dest + printed, sz, "%p", p);
    AdvanceDest(printedsz);
}

void ToBuffer::Print(void* dest, const long long* p)
{
    int sz = snprintf(NULL, 0ull, "%p", p) + 1;
    BoundCheck(sz);
    size_t printedsz = snprintf((char*) dest + printed, sz, "%p", p);
    AdvanceDest(printedsz);
}

void ToBuffer::Print(void* dest, const unsigned long long* p)
{
    int sz = snprintf(NULL, 0ull, "%p", p) + 1;
    BoundCheck(sz);
    size_t printedsz = snprintf((char*) dest + printed, sz, "%p", p);
    AdvanceDest(printedsz);
}

void ToBuffer::Print(void* dest, const void* p)
{
    int sz = snprintf(NULL, 0ull, "%p", p) + 1;
    BoundCheck(sz);
    size_t printedsz = snprintf((char*) dest + printed, sz, "%p", p);
    AdvanceDest(printedsz);
}

void ToBuffer::PrintFmt(void* dest, const char* fmt, va_list argp)
{
    va_list copied;
    va_copy(copied, argp);
    int sz = vsnprintf(NULL, 0ull, fmt, copied) + 1;
    BoundCheck(sz);
    size_t printedsz = vsnprintf((char*) dest + printed, sz, fmt, argp);
    AdvanceDest(printedsz);
}

ToIndentedBuffer::ToIndentedBuffer(const size_t bufSize, const unsigned int indentSize) : ToBuffer(bufSize), indentationSize(indentSize) {}

void ToIndentedBuffer::BeforePrint(void* dest)
{
    if (indentationSize) {
        BoundCheck(indentationSize);
        sprintf((char*) dest, "%*s", indentationSize, "");
        AdvanceDest(indentationSize - 1);
        indentationSize = 0;
    }
}

Out::Out(void* destStream) : dest(destStream), outputStrategy(nullptr)
{
    static std::shared_ptr<OutputStrategy> toFile = std::make_shared<ToFile>();
    outputStrategy = toFile;
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

const void* Out::Flush() const
{
    return outputStrategy->Flush(dest);
}

}; // namespace OutStream
