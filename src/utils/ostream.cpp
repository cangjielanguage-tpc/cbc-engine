#include "utils/ostream.h"

#include "engine/resolving_output.h"
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
class IOSPlatformLogOutput : public Output {
public:
    IOSPlatformLogOutput() : Output(), buffer() {}

    void Flush() const override;
    void NewLine() override;
    void VPrintFmt(const char* fmt, va_list argp) override;

private:
    mutable StringBuffer buffer; // mutable because Flush() is const
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
    *this << "\n";
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

Output& Output::operator<<(const float v)
{
    PrintFmt("%f", v);
    return *this;
}

Output& Output::operator<<(const double v)
{
    PrintFmt("%f", v);
    return *this;
}

Output& Output::operator<<(const long double v)
{
    PrintFmt("%Lf", v);
    return *this;
}

Output& Output::operator<<(const bool v)
{
    PrintFmt("%s", v ? "true" : "false");
    return *this;
}

Output& Output::operator<<(const signed char v)
{
    PrintFmt("%d", v);
    return *this;
}

Output& Output::operator<<(const unsigned char v)
{
    PrintFmt("%u", v);
    return *this;
}

Output& Output::operator<<(const short v)
{
    PrintFmt("%d", v);
    return *this;
}

Output& Output::operator<<(const unsigned short v)
{
    PrintFmt("%u", v);
    return *this;
}

Output& Output::operator<<(const int v)
{
    PrintFmt("%d", v);
    return *this;
}

Output& Output::operator<<(const unsigned int v)
{
    PrintFmt("%u", v);
    return *this;
}

Output& Output::operator<<(const long v)
{
    PrintFmt("%ld", v);
    return *this;
}

Output& Output::operator<<(const unsigned long v)
{
    PrintFmt("%ld", v);
    return *this;
}

Output& Output::operator<<(const long long v)
{
    PrintFmt("%lld", v);
    return *this;
}

Output& Output::operator<<(const unsigned long long v)
{
    PrintFmt("%llu", v);
    return *this;
}

Output& Output::operator<<(const char c)
{
    PrintFmt("%c", c);
    return *this;
}

Output& Output::operator<<(const char* cstr)
{
    PrintFmt("%s", cstr);
    return *this;
}

Output& Output::operator<<(const std::string_view strv)
{
    PrintFmt("%.*s", static_cast<int>(strv.length()), strv.data());
    return *this;
}

Output& Output::operator<<(const std::string str)
{
    PrintFmt("%s", str.c_str());
    return *this;
}

Output& Output::operator<<(const signed char* p)
{
    PrintFmt("%p", p);
    return *this;
}

Output& Output::operator<<(const unsigned char* p)
{
    PrintFmt("%p", p);
    return *this;
}

Output& Output::operator<<(const short* p)
{
    PrintFmt("%p", p);
    return *this;
}

Output& Output::operator<<(const unsigned short* p)
{
    PrintFmt("%p", p);
    return *this;
}

Output& Output::operator<<(const int* p)
{
    PrintFmt("%p", p);
    return *this;
}

Output& Output::operator<<(const unsigned int* p)
{
    PrintFmt("%p", p);
    return *this;
}

Output& Output::operator<<(const long* p)
{
    PrintFmt("%p", p);
    return *this;
}

Output& Output::operator<<(const unsigned long* p)
{
    PrintFmt("%p", p);
    return *this;
}

Output& Output::operator<<(const long long* p)
{
    PrintFmt("%p", p);
    return *this;
}

Output& Output::operator<<(const unsigned long long* p)
{
    PrintFmt("%p", p);
    return *this;
}

Output& Output::operator<<(const void* p)
{
    PrintFmt("%p", p);
    return *this;
}

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
    if (buffer.Size() == 0) {
        return;
    }

    char* message = buffer.ToCString();
    if (message != nullptr) {
        os_log(OS_LOG_DEFAULT, "%{public}s", message);
        free(message);
    }

    buffer.Clear();
}

void IOSPlatformLogOutput::NewLine() { Flush(); }

void IOSPlatformLogOutput::VPrintFmt(const char* fmt, va_list argp) { buffer.VPrintFmt(fmt, argp); }
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

template <typename Out> inline void DoPrint(Out& out, std::string_view string)
{
    while (true) {
        size_t pos = string.find("\n");
        if (pos == std::string_view::npos) {
            out << string;
            return;
        } else {
            out << string.substr(0, pos);
            out.NewLine();
            string = string.substr(pos + 1);
        }
    }
}

template <typename Out>
static void DoPrintImpl(
    Out& out, std::string_view fmt, FormatValue<Out> const* values, FormatHandler<Out> const* handlers, int count
)
{
    int idx    = 0;
    size_t pos = 0;
    while (pos < fmt.size()) {
        size_t next = fmt.find("{}", pos);
        if (next == std::string_view::npos) {
            DoPrint(out, fmt.substr(pos));
            return;
        }
        DoPrint(out, fmt.substr(pos, next - pos));
        if (idx < count) {
            handlers[idx](values[idx], out);
            idx++;
        }
        pos = next + 2;
    }
}

template <>
[[gnu::noinline]] void DoPrintArray<Output>(
    Output& out,
    std::string_view fmt,
    FormatValue<Output> const* values,
    FormatHandler<Output> const* handlers,
    int count
)
{
    DoPrintImpl(out, fmt, values, handlers, count);
}

template <>
[[gnu::noinline]] void DoPrintArray<ResolvingOutput>(
    ResolvingOutput& out,
    std::string_view fmt,
    FormatValue<ResolvingOutput> const* values,
    FormatHandler<ResolvingOutput> const* handlers,
    int count
)
{
    DoPrintImpl(out, fmt, values, handlers, count);
}

Stream::Output& operator<<(Stream::Output& stream, Hex num)
{
    stream.PrintFmt("%llx", num.num);
    return stream;
}

}; // namespace Stream
