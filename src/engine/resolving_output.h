#pragma once

#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/symlevel/io/file_id.h"
#include "engine/symlevel/method_table.h"
#include "engine/symlevel/reader.h"
#include "engine/symlevel/string.h"
#include "engine/terms.h"
#include "utils/ostream.h"

namespace Stream {

template <typename T> struct Detailed {
    T value;

    Detailed(T value) : value(value) {}
};

class ResolvingOutput {
public:
    ResolvingOutput(Engine::Session& session, Stream::Output& out);

    ResolvingOutput& operator<<(Engine::Term term);
    ResolvingOutput& operator<<(Engine::GlobalTerm term);
    ResolvingOutput& operator<<(Engine::LocalTerm term);
    ResolvingOutput& operator<<(IO::FileId fileId);
    ResolvingOutput& operator<<(Detailed<Engine::RefIdentifier<Engine::Term>> id);
    ResolvingOutput& operator<<(Symlevel::MethodTable const& mt);

    template <typename T> ResolvingOutput& operator<<(Engine::Identifier<T> id)
    {
        return *this << "(" << id.GetFileId() << "," << id.GetOffset() << ")";
    }

    template <typename T> ResolvingOutput& operator<<(Engine::RefIdentifier<T> id)
    {
        return *this << "<" << id.GetFileId() << "," << id.GetIndex().GetRegion() << "," << id.GetIndex().GetIndex()
                     << ">";
    }

    template <typename T> ResolvingOutput& operator<<(Detailed<Engine::Identifier<T>> id)
    {
        return *this << Symlevel::Reader::Read(session, id.value);
    }

    template <typename T> ResolvingOutput& operator<<(const T v)
    {
        out << v;
        return *this;
    }

    template <typename T> ResolvingOutput& operator<<(Detailed<T> v) { return *this << v.value; }

    Engine::Session& session;
    Stream::Output& out;
};

} // namespace Stream
