#include "disasmer.h"
#include "engine/identifiers.h"
#include "engine/symlevel/io/filesystem.h"
#include "engine/symlevel/io/stream_file_reader.h"
#include "engine/symlevel/offset.h"
#include "engine/symlevel/references.h"
#include "engine/symlevel/region_data.h"
#include "engine/symlevel/sequence.h"
#include <cstdint>
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

void Disasmer::RData(RegionData const& rd, uint8_t regionNum)
{
    auto& raf = *session->FileOf(currentFile->Id());
    Region("methods: ", [&]() {
        auto mrefs = rd.MethodReferencesOffsets().Offsets(raf);
        for (auto ref : mrefs) {
            auto offset = IO::StreamFileReader(raf, ref).ReadULEB();
            auto refid  = RefId<MethodReference>(regionNum, offset);

            auto methodIdx       = RefIdentifier(refid, currentFile->Id());
            auto methodReference = MethodReference::Parse(*session, methodIdx);

            io << "method offset: " << offset << ", method: " << Detailed(methodReference.name) << endl;
        }
    });

    Region("terms: ", [&]() {
        auto terms = rd.TermsOffsets().Offsets(raf);
        for (auto ref : terms) {
            // TODO
            auto offset = IO::StreamFileReader(raf, ref).ReadULEB();
            auto refid  = RefId<Term>(regionNum, offset);

            auto termIdx = RefIdentifier(refid, currentFile->Id());
            auto term    = TermManager::Resolve(*session, termIdx);

            io << "term offset: " << offset << ", term: " << term << endl;
        }
    });

    Region("fields: ", [&]() {
        auto frefs = rd.FieldReferencesOffsets().Offsets(raf);
        for (auto ref : frefs) {
            auto offset = IO::StreamFileReader(raf, ref).ReadULEB();
            auto refid  = RefId<FieldReference>(regionNum, offset);

            auto fieldIdx       = RefIdentifier(refid, currentFile->Id());
            auto fieldReference = FieldReference::Parse(*session, fieldIdx);

            io << "field offset: " << offset << ", field: " << Detailed(fieldReference.name) << endl;
        }
    });
}

void Disasmer::DisasmOf(CbcFile& file)
{
    SetFile(file);
    Version(file.GetVersionMetadata());

    auto ti = file.GetTypeIndex();
    Region("types", [&]() { ti.ForEach(*session, [&](TypeDefinition& def) { Type(def); }); });
    Region("region data", [&]() { RData(file.GetRegionData(), 0); });
}

} // namespace Dis
