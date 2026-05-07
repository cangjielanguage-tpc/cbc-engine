#pragma once

#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/symlevel/reader.h"
#include "engine/symlevel/string.h"
#include "engine/terms.h"
#include "utils/ostream.h"

namespace Engine {

class ResolvingOutput {
public:
    ResolvingOutput(Session& session, Stream::Output& out);

    ResolvingOutput& operator<<(RefIdentifier<Term> term);
    ResolvingOutput& operator<<(Term term);
    ResolvingOutput& operator<<(GlobalTerm term);
    ResolvingOutput& operator<<(LocalTerm term);

    template <typename T>
    ResolvingOutput& operator<<(Identifier<T> id)
    {
        return *this << Symlevel::Reader::Read(session, id);
    }

    template <typename T> ResolvingOutput& operator<<(const T v)
    {
        out << v;
        return *this;
    }

    Session& session;
    Stream::Output& out;
};

}
