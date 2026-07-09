#include "utils/ostream.h"

#include "utils/assertion.h"
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
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

#if CBC_ENGINE_STREAM_IOS_OS_LOG
constexpr size_t PLATFORM_LOG_MAX_MESSAGE_SIZE = 4096;

class IOSPlatformLogOutput : public Output {
public:
    IOSPlatformLogOutput() : Output(), buffer(), bufferSize(0) {}

    void Flush() const override;
    void NewLine() override;
    void VPrintFmt(const char* fmt, va_list argp) override;

private:
    mutable StringBuffer buffer; // mutable because Flush() is const
    mutable size_t bufferSize;
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

#if CBC_ENGINE_STREAM_IOS_OS_LOG
void IOSPlatformLogOutput::Flush() const
{
    if (bufferSize == 0) {
        return;
    }

    char* message = buffer.ToCString();
    if (message != nullptr) {
        os_log(OS_LOG_DEFAULT, "%{public}s", message);
        free(message);
    }

    buffer.Clear();
    bufferSize = 0;
}

void IOSPlatformLogOutput::NewLine() { Flush(); }

void IOSPlatformLogOutput::VPrintFmt(const char* fmt, va_list argp)
{
    char message[PLATFORM_LOG_MAX_MESSAGE_SIZE];
    int written = vsnprintf(message, sizeof(message), fmt, argp);
    if (written < 0) {
        return;
    }

    size_t messageSize = static_cast<size_t>(written);
    messageSize        = std::min(messageSize, sizeof(message) - 1);

    auto available  = PLATFORM_LOG_MAX_MESSAGE_SIZE - 1 - bufferSize;
    auto appendSize = std::min(messageSize, available);
    if (appendSize == 0) {
        return;
    }

    buffer.PrintFmt("%.*s", static_cast<int>(appendSize), message);
    bufferSize += appendSize;
}
#endif

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

Indented::Indented(Output& astream, const unsigned int indentSize) : stream(astream), indentationSize(indentSize) {}

void Indented::NewLine()
{
    stream.NewLine();
    newLine = true;
}

void Indented::Flush() const { stream.Flush(); }

void Indented::SetIndent(unsigned int indent) { indentationSize = indent; }

unsigned int Indented::GetIndent() const { return indentationSize; }

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
