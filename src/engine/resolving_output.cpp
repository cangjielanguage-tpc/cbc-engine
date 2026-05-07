#include "resolving_output.h"
#include "engine/identifiers.h"
#include "engine/terms.h"

namespace Stream {

ResolvingOutput::ResolvingOutput(Engine::Session& session, Stream::Output& out)
    : session(session), out(out) {}

ResolvingOutput& ResolvingOutput::operator<<(Engine::Term term)
{
    return *this << term.GetName(session);
}

ResolvingOutput& ResolvingOutput::operator<<(Detailed<Engine::RefIdentifier<Engine::Term>> term)
{
    return *this << term.value << Engine::TermManager::Resolve(session, term.value);
}

ResolvingOutput& ResolvingOutput::operator<<(Engine::GlobalTerm term)
{
    return *this << Engine::Term(term);
}

ResolvingOutput& ResolvingOutput::operator<<(Engine::LocalTerm term)
{
    return *this << Engine::Term(term);
}

ResolvingOutput& ResolvingOutput::operator<<(IO::FileId fileId)
{
    return *this << fileId.id;
}

}
