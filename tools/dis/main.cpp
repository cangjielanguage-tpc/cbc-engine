#include "cli.h"
#include "engine/engine.h"
#include "engine/symlevel/cbc_file.h"
#include "engine/symlevel/io/filesystem.h"
#include "engine/symlevel/terms.h"
#include <filesystem>
#include <memory>
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

    void DisasmOf(Symlevel::CbcFile& file) { file }

    std::unique_ptr<Engine::Session> session;
    std::vector<Symlevel::CbcFile>& files;

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
