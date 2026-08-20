#include "resolving_output.h"
#include "cbc/isa_disasm.h"
#include "engine/decode/decoder.h"
#include "engine/engine.h"
#include "engine/field_layout.h"
#include "engine/identifiers.h"
#include "engine/method_table.h"
#include "engine/image/reader.h"
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

ResolvingOutput& ResolvingOutput::operator<<(Engine::Term term)
{
    term.GetName(session, out);
    return *this;
}

ResolvingOutput& ResolvingOutput::operator<<(Detailed<Image::RefIdentifier<Engine::Term>> term)
{
    return *this << Engine::TermManager::Resolve(session, term.value);
}

ResolvingOutput& ResolvingOutput::operator<<(Engine::GlobalTerm term) { return *this << Engine::Term(term); }

ResolvingOutput& ResolvingOutput::operator<<(Engine::LocalTerm term) { return *this << Engine::Term(term); }

ResolvingOutput& ResolvingOutput::operator<<(Image::FileId fileId) { return *this << fileId.id; }

ResolvingOutput& ResolvingOutput::operator<<(Image::FieldDefinition const& fd)
{
    auto& out  = *this;
    auto ftype = Detailed(fd.FieldType());
    auto name  = StringOf(fd.GetName());
    out << Detailed(fd.Flags()) << " " << Detailed(fd.FieldType()) << " " << name;
    return out;
}

ResolvingOutput& ResolvingOutput::operator<<(NoResolve<Image::FieldDefinition> nr)
{
    auto& out  = *this;
    auto& fd   = nr.value;
    auto ftype = fd.FieldType();
    auto name  = fd.GetName();
    out << Detailed(fd.Flags()) << ". field type: " << ftype << ". name: " << name;
    return out;
}

ResolvingOutput& ResolvingOutput::operator<<(Image::Code const& code)
{
    using namespace Stream;
    Stream::Indented out2(out, 2);
    Stream::Indented out4(out, 4);

    out << "MethodCode {" << endl;
    out2 << "untypedSlotCount: " << code.untypedSlotCount << endl
         << "stackAllocSigsCount: " << code.stackAllocSigsCount << endl
         << "stackAllocSigs: ";

    for (size_t i = 0; i < code.stackAllocSigsCount; i++) {
        out2 << code.stackAllocSigs[i];
        if (i < (code.stackAllocSigsCount - 1)) {
            out2 << ", ";
        }
    }
    out2 << endl;

    out2 << "ohmSlotCount: " << code.ohmSlotCount << endl
         << "usedNonVolIRegMask: " << code.usedNonVolIRegMask << endl
         << "usedNonVolFRegMask: " << code.usedNonVolFRegMask << endl
         << "maxCalleeStackArgsCount: " << code.maxCalleeStackArgsCount << endl;

    out2 << "ExceptionTable {" << endl;
    for (const auto& [start, end, target] : Image::Reader::GetExceptionRegions(session, code)) {
        out2 << "  [" << start << ", " << end << ") -> " << target << endl;
    }
    out2 << "}" << endl;

    out2 << "LivenessInfo {" << endl;
    for (const auto& li : Image::Reader::GetLivenessInfo(session, code)) {
        out4 << "cbcPos: " << li.cbcPos << ", regMask: " << li.regMask << ", ";
        Std::Vector::Print(out4, li.refSlotNums);
        out4 << ", ";
        Std::Vector::Print(out4, li.mutPairs);
        out4 << endl;
    }
    out2 << "}" << endl;

    out2 << "StackPtrsInfo {" << endl;
    for (const auto& spi : Image::Reader::GetStackPtrsInfo(session, code)) {
        out4 << "cbcPos: " << spi.cbcPos << ", ";
        Std::Vector::Print(out4, spi.resources);
        out4 << endl;
    }
    out2 << "}" << endl;

    out << "}" << endl;
    return *this;
}

ResolvingOutput& ResolvingOutput::operator<<(Full<Image::MethodDefinition> full)
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
                auto code = Image::Reader::Read(session, md.FileId(), codeOpt->GetOffset());
                out << code;
                Cbc::Disasm(ResolvingOutput::out, code, &resolver);
            });
        }
    });

    return out;
}

ResolvingOutput& ResolvingOutput::operator<<(Image::MethodDefinition const& md)
{
    Print("{}.{}{}", Detailed(md.TypeName()), Detailed(md.Name()), Detailed(md.Signature()));
    return *this;
}

ResolvingOutput& ResolvingOutput::operator<<(NoResolve<Image::MethodDefinition> nr)
{
    auto& out = *this;
    auto& md  = nr.value;
    out << "name: " << md.Name().GetOffset() << ". sig: " << md.Signature();
    return out;
}

ResolvingOutput& ResolvingOutput::operator<<(NoResolve<Image::TypeDefinition> nr)
{
    auto& out = *this;
    auto& td  = nr.value;
    Region("method name: " + std::to_string(td.GetName().GetOffset()), [&]() {
        out << "super: " << td.GetSuperType() << endl;
        Region("fields", [&]() {
            for (auto id : Image::Reader::AllEntries(session, td.GetFields())) {
                out << NoResolve(id) << endl;
            }
        });
        Region("methods", [&]() {
            for (auto id : Image::Reader::AllEntries(session, td.GetMethods())) {
                out << NoResolve(id);
            }
        });
        Region("virtual methods", [&]() {
            auto vms = td.GetVirtualMethods();
            for (auto ident : Image::Reader::Resolve(session, vms)) {
                out << NoResolve(ident);
            }
        });
    });
    return out;
}

ResolvingOutput& ResolvingOutput::TypeDefinition(Image::TypeDefinition const& td, bool full)
{
    auto& out = *this;
    Region(StringOf(td.GetName()), [&]() {
        out << "super: " << Detailed(td.GetSuperType()) << endl;

        Region("interfaces", [&]() {
            for (auto id : Image::Reader::Resolve(session, td.GetInterfaces())) {
                out << Detailed(id) << endl;
            }
        });

        Region("fields", [&]() {
            for (auto field : Image::Reader::AllEntries(session, td.GetFields())) {
                out << Detailed(field) << endl;
            }
        });

        Region("instance fields", [&]() {
            for (auto id : Image::Reader::Resolve(session, td.GetInstanceFields())) {
                out << Detailed(id) << endl;
            }
        });

        Region("methods", [&]() {
            for (auto id : Image::Reader::AllEntries(session, td.GetMethods())) {
                if (full) {
                    out << Full(id);
                } else {
                    out << Detailed(id);
                }
            }
        });

        Region("virtual methods", [&]() {
            auto vms = td.GetVirtualMethods();
            for (auto ident : Image::Reader::Resolve(session, vms)) {
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

ResolvingOutput& ResolvingOutput::operator<<(Full<Image::TypeDefinition> full)
{
    return TypeDefinition(full.value, true);
}

ResolvingOutput& ResolvingOutput::operator<<(Image::TypeDefinition const& td) { return TypeDefinition(td, false); }

ResolvingOutput& ResolvingOutput::operator<<(Engine::MethodTable const& mt)
{
    auto& out = *this;
    out << "method table:" << endl;
    out << "  classes:" << endl;

    auto writeEntry = [&](Engine::MethodTableEntry& entry) {
        auto def = Image::Reader::Read(session, entry.method);

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
            auto def = Image::Reader::Read(session, *f.definition);
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

Image::String ResolvingOutput::StringOf(Image::Offset<Image::String> str, Image::FileId fid)
{
    return Image::Reader::Read(session, fid, str);
}

Image::String ResolvingOutput::StringOf(Image::Identifier<Image::String> str)
{
    return Image::Reader::Read(session, str);
}

} // namespace Stream
