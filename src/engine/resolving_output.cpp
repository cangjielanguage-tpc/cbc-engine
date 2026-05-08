#include "resolving_output.h"
#include "engine/identifiers.h"
#include "engine/symlevel/method_table.h"
#include "engine/symlevel/reader.h"
#include "engine/terms.h"

namespace Stream {

ResolvingOutput::ResolvingOutput(Engine::Session& session, Stream::Output& out) : session(session), out(out) {}

ResolvingOutput& ResolvingOutput::operator<<(Engine::Term term) { return *this << term.GetName(session); }

ResolvingOutput& ResolvingOutput::operator<<(Detailed<Engine::RefIdentifier<Engine::Term>> term)
{
    return *this << Engine::TermManager::Resolve(session, term.value) << " " << term.value;
}

ResolvingOutput& ResolvingOutput::operator<<(Engine::GlobalTerm term) { return *this << Engine::Term(term); }

ResolvingOutput& ResolvingOutput::operator<<(Engine::LocalTerm term) { return *this << Engine::Term(term); }

ResolvingOutput& ResolvingOutput::operator<<(IO::FileId fileId) { return *this << fileId.id; }

ResolvingOutput& ResolvingOutput::operator<<(Symlevel::MethodTable const& mt)
{
    auto& out = *this;
    out << "method table:" << endl;
    out << "  classes:" << endl;

    auto writeEntry = [&](Symlevel::MethodTableEntry& entry) {
        auto def = Symlevel::Reader::Read(session, entry.method);
        out << "      " << entry.methodNum << ": ";
        out << Detailed(def.Name()) << Detailed(def.Signature());
        out << ", from: " << entry.genericContext << endl;
    };

    auto writeTable = [&](Symlevel::MethodSubTable& st) {
        out << "    " << st.DeclaringType();
        out << " [" << st.StartPos() << ", " << st.EndPos() << "]:" << endl;
        for (auto entry : st.Entries()) {
            writeEntry(entry);
        }
    };

    for (auto st : mt.Classes()) {
        writeTable(st);
    }
    out << "  interfaces:" << endl;
    for (auto st : mt.Interfaces()) {
        writeTable(st);
    }
    return out;
}

} // namespace Stream
