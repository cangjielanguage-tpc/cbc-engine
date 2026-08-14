#include "disasmer.h"
#include "engine/decode/decoder.h"
#include "engine/identifiers.h"
#include "engine/resolving_output.h"
#include "engine/symlevel/io/filesystem.h"
#include "engine/symlevel/io/stream_file_reader.h"
#include "engine/symlevel/reader.h"
#include <cstdint>
#include <memory>

namespace Dis {

using namespace Engine;
using namespace Symlevel;
using namespace Stream;

Session Disasmer::SessionFor(std::vector<std::string_view> views)
{
    auto loader = Loader();
    for (auto view : views) {
        auto raf = IO::OpenFile(std::string(view));
        if (raf.has_value()) {
            loader.Load(std::move(raf.value()), view);
        }
    }
    return Session(loader.Build());
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
    auto& raf = session.FileOf(currentFile->Id());
    Region("methods: ", [&]() {
        for (auto refid : rd.methods) {
            auto ref = Reader::Read(session, refid);

            io << refid.GetIndex() << " - " << Detailed(ref.refType) << "." << Detailed(ref.name)
               << Detailed(ref.methodSig) << ' ' << ref.flags << endl;
        }
    });

    Region("terms: ", [&]() {
        for (auto refid : rd.terms) {
            auto term = TermManager::Resolve(session, refid);

            io << refid.GetIndex() << " - " << term << endl;
        }
    });

    Region("fields: ", [&]() {
        for (auto refid : rd.fields) {
            auto ref = Reader::Read(session, refid);

            io << refid.GetIndex() << " - " << Detailed(ref.refType) << "." << Detailed(ref.name) << " "
               << Detailed(ref.fieldType) << endl;
        }
    });
}

void Disasmer::DisasmOf(CbcFile const& file)
{
    SetFile(file);
    Version(file.GetVersionMetadata());

    Region("types", [&]() {
        for (auto type : session.Decoder().AllEntries(file.GetTypeIndex())) {
            auto def = Symlevel::Reader::Read(session, type);
            Type(def);
        }
    });
    Region("region data", [&]() { RData(file.GetRegionData(), 0); });
}

} // namespace Dis
