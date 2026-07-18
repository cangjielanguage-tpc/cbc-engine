#include "resolving_output.h"
#include "cbc/isa_disasm.h"
#include "engine/engine.h"
#include "engine/field_layout.h"
#include "engine/identifiers.h"
#include "engine/method_table.h"
#include "engine/symlevel/code.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/io/file_id.h"
#include "engine/symlevel/reader.h"
#include "engine/terms.h"
#include "resolution/resolution.h"

namespace Stream {

ResolvingOutput::ResolvingOutput(Engine::Session& session, Stream::Output& out)
    : session(session),
      holder(Indented(out, 0))
{}

template <typename T> ResolvingOutput& operator<<(ResolvingOutput& out, std::optional<T> opt)
{
    if (opt.has_value()) {
        out << *opt;
    } else {
        out << "<none>";
    }
    return out;
}

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
    auto name  = StringOf(fd.GetName());
    out << Detailed(fd.Flags()) << " " << Detailed(fd.FieldType()) << " " << name;
    return out;
}

ResolvingOutput& ResolvingOutput::operator<<(NoResolve<Symlevel::FieldDefinition> nr)
{
    auto& out  = *this;
    auto& fd   = nr.value;
    auto ftype = fd.FieldType();
    auto name  = fd.GetName();
    out << Detailed(fd.Flags()) << ". field type: " << ftype << ". name: " << name;
    return out;
}

ResolvingOutput& ResolvingOutput::operator<<(Full<Symlevel::MethodDefinition> full)
{
    auto& md      = full.value;
    auto& out     = *this;
    auto resolver = Resolution::Resolver(session, md.GetIdentifier());
    auto name     = Detailed(md.Name());
    out << name << Detailed(md.Signature()) << " ";

    Region("", [&] {
        out << "flags: " << Detailed(md.GetFlags()) << endl;
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
            for (auto field : td.GetFields().Entries(session)) {
                out << NoResolve(field) << endl;
            }
        });
        Region("methods", [&]() {
            for (auto id : td.GetMethods().Entries(session)) {
                out << NoResolve(id);
            }
        });
        Region("virtual methods", [&]() {
            auto vms = td.GetVirtualMethods();
            for (auto ident : vms.Values(session)) {
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

        Region("interfaces", [&]() {
            for (auto id : td.GetInterfaces().Values(session)) {
                out << Detailed(id) << endl;
            }
        });

        Region("fields", [&]() {
            for (auto field : td.GetFields().Entries(session)) {
                out << Detailed(field) << endl;
            }
        });

        Region("instance fields", [&]() {
            for (auto id : td.GetInstanceFields().Values(session)) {
                out << Detailed(id) << endl;
            }
        });

        Region("methods", [&]() {
            for (auto id : td.GetMethods().Entries(session)) {
                if (full) {
                    out << Full(id);
                } else {
                    out << Detailed(id);
                }
            }
        });

        Region("virtual methods", [&]() {
            auto vms = td.GetVirtualMethods();
            for (auto ident : vms.Values(session)) {
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

ResolvingOutput& ResolvingOutput::operator<<(Engine::MethodTable const& mt)
{
    auto& out = *this;
    out << "method table:" << endl;
    out << "  classes:" << endl;

    auto writeEntry = [&](Engine::MethodTableEntry& entry) {
        auto def = Symlevel::Reader::Read(session, entry.method);

        Engine::MethodSignatureSubstitution sub(session, entry.genericContext);
        auto signature = Engine::TermManager::Resolve(session, def.Signature());
        signature = sub.Substitute(signature);

        out << "      " << entry.methodNum << ": ";
        out << Detailed(def.Name()) << signature;
        out << ", from: " << entry.genericContext << endl;
    };

    auto writeTable = [&](Engine::MethodSubTable& st) {
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

ResolvingOutput& ResolvingOutput::operator<<(Engine::FieldLayout const& layout)
{
    auto& stream = *this;
    stream << "field layout:" << endl;

    out.SetIndent(out.GetIndent() + 2);

    stream << "size: " << layout->desc.size << endl;
    stream << "alignment: " << layout->desc.alignment << endl;

    for (auto& f : layout->fields) {
        if (f.definition) {
            auto def = Symlevel::Reader::Read(session, *f.definition);
            stream << Detailed(def.GetName()) << ": " << f.fieldType << " - " << f.offset << endl;
        } else {
            stream << "<unknown>" << ": " << f.fieldType << " - " << f.offset << endl;
        }
    }
    out.SetIndent(out.GetIndent() - 2);
    return stream;
}

template <typename T> void ResolvingOutput::Region(T name, std::function<void()> f)
{
    *this << name << " {" << endl;
    out.SetIndent(out.GetIndent() + 2);
    f();
    out.SetIndent(out.GetIndent() - 2);
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
