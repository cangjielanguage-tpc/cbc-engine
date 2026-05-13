#include "disasmer.h"
#include "engine/identifiers.h"
#include "engine/symlevel/io/filesystem.h"
#include "engine/symlevel/offset.h"
#include "engine/symlevel/offset_sequence.h"
#include "engine/symlevel/references.h"
#include "engine/symlevel/region_data.h"
#include <memory>

namespace Dis {

using namespace Engine;
using namespace Symlevel;
using namespace Stream;

std::unique_ptr<Session> Disasmer::SessionFor(std::vector<std::string_view> views)
{
    auto loader = Loader();
    for (auto view : views) {
        auto raf = IO::OpenFile(std::filesystem::path(view));
        loader.Load(std::move(raf), view);
    }
    return std::make_unique<Session>(loader.Build());
}

void Disasmer::Version(const VersionMetadata& md)
{
    io << "File version: ";
    io << (unsigned int)md.fileVersion;
    io << ". Bytecode version: ";
    io << (unsigned int)md.bytecodeVersion << ".";
    io << endl;
}

void Disasmer::Region(String name, std::function<void()> fn)
{
    io << name << " {" << endl;
    idio.SetIndent([](auto indent) { return indent + 2; });
    fn();
    idio.SetIndent([](auto indent) { return indent - 2; });
    io << "}" << endl;
}

void Disasmer::Type(TypeDefinition& def)
{
    if (resolving) {
        io << Full(def.GetIdentifier()) << endl;
    } else {
        io << NoResolve(def.GetIdentifier()) << endl;
    }
}

void Disasmer::RData(RegionData const& rd)
{
    Region("methods: ", [&]() {
        rd.MethodReferencesOffsets().ForEach(
            *session->FileOf(currentFile->Id()), [&](Symlevel::RefId<MethodReference> mr) {
                auto metr = MethodReference::Parse(*session, RefIdentifier(mr, currentFile->Id()));
                io << "refid: " << mr.GetIndex() << ", method: " << Detailed(metr.name);
            }
        );
    });

    Region("terms: ", [&]() {
        rd.TermsOffsets().ForEach(*session->FileOf(currentFile->Id()), [&](Symlevel::RefId<Term> tr) {
            auto term = TermManager::Resolve(*session, RefIdentifier(tr, currentFile->Id()));
            io << "term refid: " << tr.GetIndex() << ", term: " << term;
        });
    });

    Region("fields: ", [&]() {
        rd.FieldReferencesOffsets().ForEach(
            *session->FileOf(currentFile->Id()), [&](Symlevel::RefId<FieldReference> tr) {
                auto field = FieldReference::Parse(*session, RefIdentifier(tr, currentFile->Id()));
                io << "field refid: " << tr.GetIndex() << ", field: " << Detailed(field.name);
            }
        );
    });
}

// TODO: maybe move that to resolving output.
void Disasmer::DisasmOf(CbcFile& file)
{
    SetFile(file);
    Version(file.GetVersionMetadata());

    auto ti = file.GetTypeIndex();
    Region("types", [&]() { ti.ForEach(*session, [&](TypeDefinition& def) { Type(def); }); });
    Region("region data", [&]() { RData(file.GetRegionData()); });
}

} // namespace Dis
