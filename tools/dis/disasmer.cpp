#include "disasmer.h"
#include "engine/identifiers.h"
#include "engine/resolving_output.h"
#include "engine/symlevel/io/filesystem.h"
#include "engine/symlevel/io/stream_file_reader.h"
#include "engine/symlevel/reader.h"
#include "engine/symlevel/references.h"
#include "engine/symlevel/region_data.h"
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
        auto raf = IO::OpenFile(std::string(view));
        if (raf.has_value()) {
            loader.Load(std::move(raf.value()), view);
        }
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
    idio.SetIndent(idio.GetIndent() + 2);
    fn();
    idio.SetIndent(idio.GetIndent() - 2);
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
    io << "region " << regionNum << endl;
    auto& raf = *session->FileOf(currentFile->Id());
    Region("methods: ", [&]() {
        auto mrefs = rd.MethodReferencesOffsets().RefIds(regionNum);
        for (auto refid : mrefs) {
            auto idx = RefIdentifier(refid, currentFile->Id());
            auto ref = MethodReference::Parse(*session, idx);

            io << refid.GetIndex() << " - " << Detailed(ref.refType) << "." << Detailed(ref.name)
               << Detailed(ref.methodSig) << endl;
        }
    });

    Region("terms: ", [&]() {
        auto terms = rd.TermsOffsets().RefIds(regionNum);
        for (auto refid : terms) {
            auto newrefid = RefId<Term>(regionNum, refid.GetIndex());
            auto termIdx  = RefIdentifier(newrefid, currentFile->Id());
            auto term     = TermManager::Resolve(*session, termIdx);

            io << newrefid.GetIndex() << " - " << term << endl;
        }
    });

    Region("fields: ", [&]() {
        auto frefs = rd.FieldReferencesOffsets().RefIds(regionNum);
        for (auto refid : frefs) {
            auto idx = RefIdentifier(refid, currentFile->Id());
            auto ref = FieldReference::Parse(*session, idx);

            io << refid.GetIndex() << " - " << Detailed(ref.refType) << "." << Detailed(ref.name) << " "
               << Detailed(ref.fieldType) << endl;
        }
    });
}

void Disasmer::DisasmOf(CbcFile const& file)
{
    SetFile(file);
    Version(file.GetVersionMetadata());

    auto ti = file.GetTypeIndex();
    Region("types", [&]() {
        for (auto type : ti.Entries(*session)) {
            auto def = Symlevel::Reader::Read(*session, type);
            Type(def);
        }
    });
    Region("region data", [&]() { RData(file.GetRegionData(), 0); });
}

} // namespace Dis
