#include "cli.h"
#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/symlevel/cbc_file.h"
#include "engine/symlevel/code.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/io/file_id.h"
#include "engine/symlevel/io/filesystem.h"
#include "engine/symlevel/offset.h"
#include "engine/symlevel/reader.h"
#include "engine/symlevel/string.h"
#include "engine/symlevel/version_metadata.h"
#include "engine/terms.h"
#include "resolution/resolution.h"
#include "utils/ostream.h"
#include <filesystem>
#include <functional>
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

    String string(Offset<String> str) { return String::Parse(*session, currentFile->Id(), str); }

    String methodName(Identifier<MethodDefinition> m)
    {
        auto mdef = MethodDefinition::Resolve(*session, m);
        return methodName(mdef);
    }

    String methodName(MethodDefinition& def) { return string(def.NameOffset()); }

    void mainF(Identifier<MethodDefinition> mainFn)
    {
        io << "Main: ";
        io << methodName(mainFn);
        io << endl;
    }

    void field(FieldDefinition& fdef, TermManager& tm)
    {
        auto term     = tm.Resolve(*session, fdef.FieldType());
        auto typeName = term.GetName(*session);
        auto flags    = fdef.Flags().ToString();
        io << "  " << flags << " " << typeName;
        io << string(fdef.NameOffset());
    }

    void region(std::string name, std::function<void()> fn) { region(String(name), fn); }

    void region(String name, std::function<void()> fn)
    {
        io << name << endl << "{" << endl;
        io.SetIndent([](auto indent) { return indent + 2; });
        fn();
        io.SetIndent([](auto indent) { return indent - 2; });
        io << "}" << endl;
    }

    String typeName(TypeDefinition& def) { return string(def.NameOffset()); }

    void type(TypeDefinition& def)
    {
        auto& tm = TermManager::Of(*session);

        region(typeName(def), [&] {
            region("fields", [&]() {
                def.GetFieldIndex().ForEach(*session, [&](FieldDefinition& fdef) { field(fdef, tm); });
                io << endl;
            });

            region("methods", [&]() {
                def.GetMethodIndex().ForEach(*session, [&](MethodDefinition& mdef) {
                    io << methodName(mdef) << endl;
                    mdef.GetCodeOffset();
                    auto code = Code::Parse(*session, currentFile->Id(), mdef.GetCodeOffset());
                    //
                });
            });

            region("virtual methods", [&]() {
                std::vector<Offset<MethodDefinition>> methods;
                auto vms = def.GetVirtualMethods();
                vms.Read(*session, methods);
                for (auto offs : methods) {
                    Identifier<MethodDefinition> def(offs, vms.FileId());
                    auto md = MethodDefinition::Resolve(*session, def);
                    io << string(md.NameOffset()) << endl;
                }
            });
        });
    }

    void DisasmOf(CbcFile& file)
    {
        setFile(file);
        version(file.GetVersionMetadata());
        auto mainOpt = session->GetEngine().FindMain(*session, file.GetPath());
        if (mainOpt.has_value()) {
            mainF(*mainOpt);
        }

        auto ti = file.GetTypeIndex();
        region("types", [&]() { ti.ForEach(*session, [&](TypeDefinition& def) { type(def); }); });
        // TODO: foreign libs, coverage
    }

    void setFile(CbcFile& file)
    {
        logs << "Disassembling file " << file.GetName() << endl;
        currentFile = &file;
    }

    bool resolving       = true;
    CbcFile* currentFile = nullptr;
    std::unique_ptr<Session> session;
    std::vector<CbcFile>& files;
    // FIXME: make cout variable via constr;
    Indented io = Indented(Stream::cout, 0);

    Output& logs = Stream::cerr; // do normal logs with flags later.

public:
    void Disasm()
    {
        for (auto& file : files) {
            DisasmOf(file);
        }
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
