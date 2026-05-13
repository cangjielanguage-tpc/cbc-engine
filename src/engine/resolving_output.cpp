#include "resolving_output.h"
#include "cbc/isa_disasm.h"
#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/symlevel/code.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/io/file_id.h"
#include "engine/symlevel/method_table.h"
#include "engine/symlevel/reader.h"
#include "engine/terms.h"
#include "resolution/resolution.h"

namespace Stream {

ResolvingOutput::ResolvingOutput(Engine::Session& session, Stream::Output& out)
    : session(session),
      holder(Indented(out))
{}

ResolvingOutput& ResolvingOutput::operator<<(Engine::Term term) { return *this << term.GetName(session); }

ResolvingOutput& ResolvingOutput::operator<<(Detailed<Engine::RefIdentifier<Engine::Term>> term)
{
    return *this << Engine::TermManager::Resolve(session, term.value);
}

ResolvingOutput& ResolvingOutput::operator<<(Engine::GlobalTerm term) { return *this << Engine::Term(term); }

ResolvingOutput& ResolvingOutput::operator<<(Engine::LocalTerm term) { return *this << Engine::Term(term); }

ResolvingOutput& ResolvingOutput::operator<<(IO::FileId fileId) { return *this << fileId.id; }

ResolvingOutput& ResolvingOutput::operator<<(Symlevel::FieldDefinition const& fd)
{
    auto& out  = *this;
    auto ftype = Detailed(fd.FieldType());
    auto name  = StringOf(fd.NameOffset(), fd.FieldType().GetFileId());
    out << Detailed(fd.Flags()) << " " << Detailed(fd.FieldType()) << " " << name;
    return out;
}

ResolvingOutput& ResolvingOutput::operator<<(NoResolve<Symlevel::FieldDefinition> nr)
{
    auto& out  = *this;
    auto& fd   = nr.value;
    auto ftype = fd.FieldType();
    auto name  = fd.NameOffset();
    out << Detailed(fd.Flags()) << ". field type: " << ftype << ". name: " << name;
    return out;
}

ResolvingOutput& ResolvingOutput::operator<<(Full<Symlevel::MethodDefinition> full)
{
    auto& md      = full.value;
    auto& out     = *this;
    auto resolver = Resolution::Resolver(session, md.GetIdentifier());
    auto name     = Detailed(md.Name());
    out << Detailed(md.Signature()) << " ";

    Region(name, [&] {
        auto sig = Detailed(md.Signature());
        out << "flags: " << Detailed(md.GetFlags()) << endl;
        out << "sig: " << Detailed(sig) << endl;
        if (auto sourceFileOpt = md.SourceFile()) {
            out << "source file: " << StringOf(*sourceFileOpt) << endl;
        }
        if (auto sourceFullOpt = md.SourceFullName()) {
            out << "source full name: " << StringOf(*sourceFullOpt) << endl;
        }
        if (auto linkageOpt = md.LinkageName()) {
            out << "linkage name: " << StringOf(*linkageOpt) << endl;
        }
        if (auto codeOpt = md.MethodCode()) {
            Region("code", [&]() {
                auto code = Symlevel::Code::Parse(session, md.FileId(), codeOpt->GetOffset());
                Cbc::Disasm(ResolvingOutput::out, code, &resolver);
            });
        }
    });

    return out;
}

ResolvingOutput& ResolvingOutput::operator<<(Symlevel::MethodDefinition const& md)
{
    auto& out = *this;
    auto name = Detailed(md.Name());
    out << name << Detailed(md.Signature());
    return out;
}

ResolvingOutput& ResolvingOutput::operator<<(NoResolve<Symlevel::MethodDefinition> nr)
{
    auto& out = *this;
    auto& md  = nr.value;
    out << "name: " << md.Name().GetOffset() << ". sig: " << md.Signature();
    return out;
}

ResolvingOutput& ResolvingOutput::operator<<(NoResolve<Symlevel::TypeDefinition> nr)
{
    auto& out = *this;
    auto& td  = nr.value;
    Region("method name: " + std::to_string(td.GetName().GetOffset()), [&]() {
        out << "super: " << td.GetSuperType() << endl;
        Region("fields", [&]() {
            td.GetFields().ForEach(session, [&](auto& def) { out << NoResolve(def.Identifier()) << endl; });
        });
        Region("methods", [&]() {
            td.GetMethods().ForEach(session, [&](auto& def) { out << NoResolve(def.GetIdentifier()); });
        });
        Region("virtual methods", [&]() {
            std::vector<Engine::Identifier<Symlevel::MethodDefinition>> methods;
            auto vms = td.GetVirtualMethods();
            vms.Read(session, methods);
            for (auto ident : methods) {
                out << NoResolve(ident);
            }
        });
    });
    return out;
}

ResolvingOutput& ResolvingOutput::TypeDefinition(Symlevel::TypeDefinition const& td, bool full)
{
    auto& out = *this;
    Region(StringOf(td.GetName()), [&]() {
        out << "super: " << Detailed(td.GetSuperType()) << endl;
        Region("fields", [&]() {
            td.GetFields().ForEach(session, [&](auto& def) { out << Detailed(def.Identifier()) << endl; });
        });

        Region("methods", [&]() {
            td.GetMethods().ForEach(session, [&](auto& def) {
                if (full) {
                    out << Full(def.GetIdentifier());
                } else {
                    out << Detailed(def.GetIdentifier());
                }
            });
        });

        Region("virtual methods", [&]() {
            std::vector<Engine::Identifier<Symlevel::MethodDefinition>> methods;
            auto vms = td.GetVirtualMethods();
            vms.Read(session, methods);
            for (auto ident : methods) {
                if (full) {
                    out << Full(ident);
                } else {
                    out << Detailed(ident);
                }
            }
        });
    });

    return out;
}

ResolvingOutput& ResolvingOutput::operator<<(Full<Symlevel::TypeDefinition> full)
{
    return TypeDefinition(full.value, true);
}

ResolvingOutput& ResolvingOutput::operator<<(Symlevel::TypeDefinition const& td) { return TypeDefinition(td, false); }

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

template <typename T> void ResolvingOutput::Region(T name, std::function<void()> f)
{
    *this << name << " {" << endl;
    out.SetIndent([](auto indent) { return indent + 2; });
    f();
    out.SetIndent([](auto indent) { return indent - 2; });
    *this << "}" << endl;
}

Symlevel::String ResolvingOutput::StringOf(Symlevel::Offset<Symlevel::String> str, IO::FileId fid)
{
    return Symlevel::String::Parse(session, fid, str);
}

Symlevel::String ResolvingOutput::StringOf(Engine::Identifier<Symlevel::String> str)
{
    return Symlevel::String::Parse(session, str);
}

} // namespace Stream
