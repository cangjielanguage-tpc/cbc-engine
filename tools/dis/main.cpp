#include "cli.h"
#include "engine/engine.h"
#include "engine/symlevel/cbc_file.h"
#include "engine/symlevel/io/filesystem.h"
#include "engine/symlevel/version_metadata.h"
#include <filesystem>
#include <iostream>
#include <memory>
#include <ostream>
#include <string_view>

namespace Dis {

class Disasmer {
    std::unique_ptr<Engine::Session> sessionFor(std::vector<std::string_view> views)
    {
        auto loader = Engine::Loader();
        for (auto view : views) {
            auto raf = IO::OpenFile(std::filesystem::path(view));
            loader.Load(std::move(raf), view);
        }
        std::unique_ptr<Engine::Session> s { new Engine::Session(loader.Build()) };
        return s;
    }

    void println(const Symlevel::VersionMetadata& md)
    {
        auto& s = *stream;
        s << "File version: ";
        s << md.fileVersion;
        s << ". Bytecode version: ";
        s << md.bytecodeVersion;
        s << std::endl;
    }

    void DisasmOf(Symlevel::CbcFile& file)
    {
        assert(this->stream != nullptr);
        println(file.GetVersionMetadata());
        //
    }

    std::unique_ptr<Engine::Session> session;
    std::vector<Symlevel::CbcFile>& files;
    std::ostream* stream = nullptr;

public:
    void Disasm(std::ostream& stream)
    {
        this->stream = &stream;
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
    disasmer.Disasm(std::cout);
}
