#pragma once

#include "engine/engine.h"
#include "engine/field_layout.h"
#include "engine/identifiers.h"
#include "engine/image/cbc_file.h"
#include "engine/image/reader.h"
#include "engine/method_table.h"
#include "engine/terms.h"
#include "utils/ostream.h"
#include <functional>

namespace Stream {

template <typename T> struct Detailed {
    T value;

    Detailed(T value) : value(value) {}
};

template <typename T> struct Full : public Detailed<T> {
    Full(T value) : Detailed<T>(value) {}
};

template <typename T> struct NoResolve : public Detailed<T> {
    NoResolve(T value) : Detailed<T>(value) {}
};

class ResolvingOutput {
public:
    ResolvingOutput(Engine::Session& session, Stream::Output& out);

    template <typename... Args> void Print(std::string_view fmt, Args const&... args)
    {
        ::Stream::DoPrint(*this, fmt, args...);
    }

    template <typename... Args> void PrintLn(std::string_view fmt, Args const&... args)
    {
        Print(fmt, args...);
        NewLine();
    }

    void NewLine() { out.NewLine(); }

    ResolvingOutput& operator<<(Engine::Term term);
    ResolvingOutput& operator<<(Engine::GlobalTerm term);
    ResolvingOutput& operator<<(Engine::LocalTerm term);
    ResolvingOutput& operator<<(Image::FileId fileId);
    ResolvingOutput& operator<<(Detailed<Image::RefIdentifier<Engine::Term>> id);
    ResolvingOutput& operator<<(Engine::MethodTable const& mt);
    ResolvingOutput& operator<<(Engine::FieldLayout const& mt);
    ResolvingOutput& operator<<(Image::FieldDefinition const& fd);
    ResolvingOutput& operator<<(NoResolve<Image::FieldDefinition> fd);
    ResolvingOutput& operator<<(Image::MethodDefinition const& md);
    ResolvingOutput& operator<<(Full<Image::MethodDefinition> md);
    ResolvingOutput& operator<<(NoResolve<Image::MethodDefinition> md);
    ResolvingOutput& operator<<(Image::TypeDefinition const& md);
    ResolvingOutput& operator<<(Full<Image::TypeDefinition> td);
    ResolvingOutput& operator<<(NoResolve<Image::TypeDefinition> td);
    ResolvingOutput& operator<<(Image::Code const& code);

    template <typename T> ResolvingOutput& operator<<(Image::Identifier<T> id)
    {
        Print("({}, {})", id.GetFileId(), id.GetOffset());
        return *this;
    }

    template <typename T> ResolvingOutput& operator<<(Image::RefIdentifier<T> id)
    {
        Print("<{}, {}>", id.GetFileId(), id.GetIndex());
        return *this;
    }

    template <typename T> ResolvingOutput& operator<<(Detailed<Image::Identifier<T>> id)
    {
        return *this << Decode::Read(session, id.value);
    }

    template <typename T> ResolvingOutput& operator<<(Full<Image::Identifier<T>> id)
    {
        return *this << Full(Decode::Read(session, id.value));
    }

    template <typename T> ResolvingOutput& operator<<(NoResolve<Image::Identifier<T>> id)
    {
        return *this << NoResolve(Decode::Read(session, id.value));
    }

    template <typename T> ResolvingOutput& operator<<(const T v)
    {
        out << v;
        return *this;
    }

    template <typename T> ResolvingOutput& operator<<(Detailed<T> v) { return *this << v.value; }

    Engine::Session& session;
    Stream::Indented& out = holder;

private:
    Stream::Indented holder;
    Image::String StringOf(Image::Offset<Image::String>, Image::FileId fid);
    Image::String StringOf(Image::Identifier<Image::String>);
    template <typename T> void Region(T name, std::function<void()> f);

    ResolvingOutput& TypeDefinition(Image::TypeDefinition const& td, bool full);
};

} // namespace Stream
