#include "disasmer.h"
#include "engine/decode/decoder.h"
#include "engine/identifiers.h"
#include "engine/image/io/filesystem.h"
#include "engine/image/io/stream_file_reader.h"
#include "engine/image/reader.h"
#include "engine/resolving_output.h"
#include <cstdint>
#include <memory>

namespace Dis {

using namespace Engine;
using namespace Image;
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

            io << refid.GetIndex() << " - " << Detailed(ref.KindAsString()) << Detailed(": ");
            switch (ref.tag) {
                case SINGLE:
                    io << Detailed(ref.single.refType) << "." << Detailed(ref.single.name) << " "
                       << Detailed(ref.single.fieldType) << endl;
                    break;
                case CONST_INDEX:
                    io << Detailed(ref.constIndex.refType) << "." << Detailed(ref.constIndex.idx) << " "
                       << Detailed(ref.constIndex.fieldType) << endl;
                    break;
                case MULTI:
                    io << "[";
                    for (uint32_t i = 0; i < ref.multi.length; i++) {
                        io << ref.multi.indices[i].GetValue();
                        if (i != ref.multi.length - 1) {
                            io << ", ";
                        }
                    }
                    io << "]" << endl;
                    break;
                case NONE: io << Detailed(ref.none.sig) << endl; break;
            }
        }
    });
}

void Disasmer::DisasmOf(CbcFile const& file)
{
    SetFile(file);

    Region("types", [&]() {
        for (auto type : Decode::AllEntries(session, file.GetTypeIndex())) {
            auto def = Decode::Read(session, type);
            Type(def);
        }
    });
    Region("region data", [&]() { RData(file.GetRegionData(), 0); });
}

} // namespace Dis
