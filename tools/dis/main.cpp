#include "engine/engine.h"
#include "engine/resolving_output.h"
#include "engine/symlevel/cbc_file.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/io/filesystem.h"
#include "engine/symlevel/string.h"
#include "engine/symlevel/version_metadata.h"
#include "utils/ostream.h"
#include <filesystem>
#include <functional>
#include <memory>
#include <string_view>

namespace Dis {

using namespace Engine;
using namespace Symlevel;
using namespace Stream;

class Disasmer {
    std::unique_ptr<Session> SessionFor(std::vector<std::string_view> views)
    {
        auto loader = Loader();
        for (auto view : views) {
            auto raf = IO::OpenFile(std::filesystem::path(view));
            loader.Load(std::move(raf), view);
        }
        std::unique_ptr<Session> s { new Session(loader.Build()) };
        return s;
    }

    void Version(const VersionMetadata& md)
    {
        io << "File version: ";
        io << (unsigned int)md.fileVersion;
        io << ". Bytecode version: ";
        io << (unsigned int)md.bytecodeVersion << ".";
        io << endl;
    }

    void Region(std::string name, std::function<void()> fn) { Region(String(name), fn); }

    void Region(String name, std::function<void()> fn)
    {
        io << name << " {" << endl;
        idio.SetIndent([](auto indent) { return indent + 2; });
        fn();
        idio.SetIndent([](auto indent) { return indent - 2; });
        io << "}" << endl;
    }

    void Type(TypeDefinition& def) { io << Full(def.GetIdentifier()) << endl; }

    void SetFile(CbcFile& file)
    {
        logs << "Disassembling file " << file.GetName() << endl;
        currentFile = &file;
    }

    CbcFile* currentFile = nullptr;
    std::unique_ptr<Session> session;
    std::vector<CbcFile>& files;
    // FIXME: make cout variable via constr;
    Indented idio;
    ResolvingOutput io = ResolvingOutput(*session, idio);

    Output& logs = Stream::cerr; // do normal logs with flags later.
                                 //

    void DisasmOf(CbcFile& file)
    {
        SetFile(file);
        Version(file.GetVersionMetadata());

        auto ti = file.GetTypeIndex();
        Region("types", [&]() { ti.ForEach(*session, [&](TypeDefinition& def) { Type(def); }); });
    }

public:
    void Disasm()
    {
        for (auto& file : files) {
            DisasmOf(file);
        }
    }

    Disasmer(std::vector<std::string_view> views, Stream::Output& s)
        : session(SessionFor(views)),
          files(session->GetEngine().files()),
          idio(s, 0)
    {}
};
} // namespace Dis

int main(int argc, char* argv[])
{
    std::string name = argv[1];
    std::vector<std::string_view> vec;
    for (size_t i = 1; i < argc; i++) {
        vec.push_back(argv[i]);
    }
    auto disasmer = Dis::Disasmer(vec, Stream::cout);
    disasmer.Disasm();
}
