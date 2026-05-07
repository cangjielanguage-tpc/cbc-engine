#include "resolving_output.h"
#include "engine/identifiers.h"
#include "engine/terms.h"

namespace Engine {

ResolvingOutput::ResolvingOutput(Session& session, Stream::Output& out)
    : session(session), out(out) {}

ResolvingOutput& ResolvingOutput::operator<<(Term term)
{
    return *this << term.GetName(session);
}

ResolvingOutput& ResolvingOutput::operator<<(RefIdentifier<Term> term)
{
    return *this << TermManager::Resolve(session, term);
}

ResolvingOutput& ResolvingOutput::operator<<(GlobalTerm term)
{
    return *this << Term(term);
}

ResolvingOutput& ResolvingOutput::operator<<(LocalTerm term)
{
    return *this << Term(term);
}

}
