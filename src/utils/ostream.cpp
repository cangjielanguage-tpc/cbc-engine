#include "utils/ostream.h"

#include "utils/assertion.h"
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#if defined(__APPLE__) && __has_include(<TargetConditionals.h>)
    #include <TargetConditionals.h>
#endif

#if defined(TARGET_OS_IOS) && TARGET_OS_IOS && __has_include(<os/log.h>)
    #include <os/log.h>
    #define CBC_ENGINE_STREAM_IOS_OS_LOG 1
#else
    #define CBC_ENGINE_STREAM_IOS_OS_LOG 0
#endif

namespace Stream {

namespace {

using ThreadBuffers = std::unordered_map<const ThreadBufferedOutput*, StringBuffer>;

ThreadBuffers& GetThreadBuffers()
{
    thread_local ThreadBuffers buffers;
    return buffers;
}

StringBuffer& GetThreadBuffer(const ThreadBufferedOutput* output) { return GetThreadBuffers()[output]; }

std::unordered_set<const Output*>& GetOpenLines()
{
    thread_local std::unordered_set<const Output*> openLines;
    return openLines;
}

bool StartLine(const Output* output) { return GetOpenLines().insert(output).second; }

void EndLine(const Output* output) { GetOpenLines().erase(output); }

#if CBC_ENGINE_STREAM_IOS_OS_LOG
class IOSPlatformLogOutput : public ThreadBufferedOutput {
public:
    void EmitLine(const std::string& line) const override;
};
#endif

} // namespace

FileOutput cout(stdout);

#if CBC_ENGINE_STREAM_IOS_OS_LOG
IOSPlatformLogOutput cerrOutput;
#else
FileOutput cerrOutput(stderr);
#endif

Output& cerr = cerrOutput;
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

void Output::PrintFmtLn(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    VPrintFmt(fmt, args);
    va_end(args);
    NewLine(); // TODO print atomically with VPrintFmt
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

void ThreadBufferedOutput::Flush() const
{
    auto& buffer = GetThreadBuffer(this);
    if (buffer.Size() == 0) {
        return;
    }

    auto line = buffer.ToString();
    buffer.Clear();
    EmitLine(line);
}

void ThreadBufferedOutput::NewLine() { Flush(); }

void ThreadBufferedOutput::VPrintFmt(const char* fmt, va_list argp) { GetThreadBuffer(this).VPrintFmt(fmt, argp); }

#if CBC_ENGINE_STREAM_IOS_OS_LOG
void IOSPlatformLogOutput::EmitLine(const std::string& line) const
{
    os_log(OS_LOG_DEFAULT, "%{public}s", line.c_str());
}
#endif

StringBuffer::StringBuffer() : data(nullptr), size(0), capacity(0) {}

void StringBuffer::Clear() { size = 0; }

size_t StringBuffer::Size() const { return size; }

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

    if (static_cast<size_t>(required) + 1 > capacity - size) {
        // extra zero should be counted
        auto newCapacity = std::max(INITIAL_CAPACITY, size + required + 1);
        newCapacity      = std::max(newCapacity, 2 * capacity);

        auto newMem = std::make_unique<char[]>(newCapacity);
        if (size != 0) {
            std::copy(data.get(), data.get() + size, newMem.get());
        }
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

char* StringBuffer::ToCString()
{
    char* mem = (char*)malloc(size + 1);
    if (!mem) {
        return nullptr;
    }
    memcpy(mem, data.get(), size);
    mem[size] = 0;
    return mem;
}

// Decorators

Indented::Indented(Output& astream, const unsigned int indentSize) : stream(astream), indentationSize(indentSize)
{
    EndLine(this);
}

void Indented::NewLine()
{
    EndLine(this);
    stream.NewLine();
}

void Indented::Flush() const { stream.Flush(); }

void Indented::SetIndent(unsigned int indent) { indentationSize = indent; }

unsigned int Indented::GetIndent() const { return indentationSize; }

void Indented::VPrintFmt(const char* fmt, va_list argp)
{
    if (StartLine(this)) {
        stream.PrintFmt("%*s", indentationSize, "");
    }
    stream.VPrintFmt(fmt, argp);
}

Descripted::Descripted(Output& astream, std::string beforeDescription) : stream(astream), beforeDesc(beforeDescription)
{
    EndLine(this);
}

void Descripted::NewLine()
{
    EndLine(this);
    stream.NewLine();
}

void Descripted::Flush() const { stream.Flush(); }

void Descripted::VPrintFmt(const char* fmt, va_list argp)
{
    if (StartLine(this)) {
        stream.PrintFmt("%s", beforeDesc.c_str());
    }
    stream.VPrintFmt(fmt, argp);
}

}; // namespace Stream
