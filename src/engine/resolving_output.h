#pragma once

#include "engine/engine.h"
#include "engine/field_layout.h"
#include "engine/identifiers.h"
#include "engine/method_table.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/io/file_id.h"
#include "engine/symlevel/offset.h"
#include "engine/symlevel/reader.h"
#include "engine/symlevel/region_data.h"
#include "engine/symlevel/string.h"
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
    ResolvingOutput& operator<<(IO::FileId fileId);
    ResolvingOutput& operator<<(Detailed<Engine::RefIdentifier<Engine::Term>> id);
    ResolvingOutput& operator<<(Engine::MethodTable const& mt);
    ResolvingOutput& operator<<(Engine::FieldLayout const& mt);
    ResolvingOutput& operator<<(Symlevel::FieldDefinition const& fd);
    ResolvingOutput& operator<<(NoResolve<Symlevel::FieldDefinition> fd);
    ResolvingOutput& operator<<(Symlevel::MethodDefinition const& md);
    ResolvingOutput& operator<<(Full<Symlevel::MethodDefinition> md);
    ResolvingOutput& operator<<(NoResolve<Symlevel::MethodDefinition> md);
    ResolvingOutput& operator<<(Symlevel::TypeDefinition const& md);
    ResolvingOutput& operator<<(Full<Symlevel::TypeDefinition> td);
    ResolvingOutput& operator<<(NoResolve<Symlevel::TypeDefinition> td);

    template <typename T> ResolvingOutput& operator<<(Engine::Identifier<T> id)
    {
        Print("({}, {})", id.GetFileId(), id.GetOffset());
        return *this;
    }

    template <typename T> ResolvingOutput& operator<<(Engine::RefIdentifier<T> id)
    {
        Print("<{}, {}>", id.GetFileId(), id.GetIndex());
        return *this;
    }

    template <typename T> ResolvingOutput& operator<<(Detailed<Engine::Identifier<T>> id)
    {
        return *this << Symlevel::Reader::Read(session, id.value);
    }

    template <typename T> ResolvingOutput& operator<<(Full<Engine::Identifier<T>> id)
    {
        return *this << Full(Symlevel::Reader::Read(session, id.value));
    }

    template <typename T> ResolvingOutput& operator<<(NoResolve<Engine::Identifier<T>> id)
    {
        return *this << NoResolve(Symlevel::Reader::Read(session, id.value));
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
    Symlevel::String StringOf(Symlevel::Offset<Symlevel::String>, IO::FileId fid);
    Symlevel::String StringOf(Engine::Identifier<Symlevel::String>);
    template <typename T> void Region(T name, std::function<void()> f);

    ResolvingOutput& TypeDefinition(Symlevel::TypeDefinition const& td, bool full);

    Decode::Decoder& Decoder() { return session.Decoder(); }
};

} // namespace Stream
