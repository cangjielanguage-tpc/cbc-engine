#include "cli.h"
#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/symlevel/cbc_file.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/io/file_id.h"
#include "engine/symlevel/io/filesystem.h"
#include "engine/symlevel/offset.h"
#include "engine/symlevel/string.h"
#include "engine/symlevel/version_metadata.h"
#include "engine/terms.h"
#include "utils/ostream.h"
#include <filesystem>
#include <iostream>
#include <memory>
#include <ostream>
#include <string_view>

namespace Dis {

using namespace Engine;
using namespace Symlevel;
using namespace Stream;

class Disasmer {
    std::unique_ptr<Session> sessionFor(std::vector<std::string_view> views)
    {
        auto loader = Loader();
        for (auto view : views) {
            auto raf = IO::OpenFile(std::filesystem::path(view));
            loader.Load(std::move(raf), view);
        }
        std::unique_ptr<Session> s { new Session(loader.Build()) };
        return s;
    }

    void version(const VersionMetadata& md)
    {
        io << "File version: ";
        io << (unsigned int)md.fileVersion;
        io << ". Bytecode version: ";
        io << (unsigned int)md.bytecodeVersion << ".";
        io << endl;
    }

    void string(Offset<String> str, IO::FileId fileId) { io << String::Parse(*session, fileId, str); }

    void methodName(Identifier<MethodDefinition> m)
    {
        auto mdef = MethodDefinition::Resolve(*session, m);
        string(mdef.NameOffset(), m.fileId); //
    }

    void mainF(Identifier<MethodDefinition> mainFn)
    {
        io << "Main: ";
        methodName(mainFn);
        io << endl;
    }

    void type(TypeDefinition& def, CbcFile& file)
    {
        auto& tm = TermManager::Of(*session);

        // name
        string(def.NameOffset(), file.Id());
        def.GetFieldIndex().ForEach(*session, [&](FieldDefinition& fdef) {
            auto term     = tm.Resolve(*session, fdef.FieldType());
            auto typeName = term.GetName(*session);
            auto flags    = fdef.Flags().ToString();
            io << "  " << flags << " " << typeName;
            string(fdef.NameOffset(), file.Id());
            io << endl;
        });
    }

    void DisasmOf(CbcFile& file)
    {
        version(file.GetVersionMetadata());
        auto mainOpt = session->GetEngine().FindMain(*session, file.GetPath());
        if (mainOpt.has_value()) {
            mainF(*mainOpt);
        }

        auto ti = file.GetTypeIndex();
        io << "types: " << endl;
        ti.ForEach(*session, [&](TypeDefinition& def) {
            string(def.NameOffset(), file.Id());
            //
        });
        // TODO: foreign libs, coverage
        // TODO: type defs
    }

    std::unique_ptr<Session> session;
    std::vector<CbcFile>& files;
    Output& io = Stream::cout;

public:
    void Disasm()
    {
        for (auto& file : files) {
            DisasmOf(file);
        }
    }

    Disasmer& withStream(Output& stream)
    {
        this->io = stream;
        return *this;
    }

    Disasmer(std::vector<std::string_view> views) : session(sessionFor(views)), files(session->GetEngine().files()) {}
};
} // namespace Dis

int main(int argc, char* argv[])
{
    // TODO: parse options etc.
    std::string name = argv[1];
    auto disasmer    = Dis::Disasmer({ name });
    disasmer.Disasm();
}
