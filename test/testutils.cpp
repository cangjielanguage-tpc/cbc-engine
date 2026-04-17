#include "testutils.h"
#include "api/resolver.h"
#include "engine/symlevel/io/filesystem.h"
#include "stdio.h"
#include <cstddef>
#include <gtest/gtest.h>
#include <memory>
#include <optional>

bool CheckForAssembler()
{
    auto jar_path = std::string(TEST_RESOURCE_DIR) + "/cbc-asm.jar";
    return std::filesystem::exists(jar_path);
}

std::unique_ptr<IO::RandomAccessFile> OpenAsm(std::string file_name)
{
    auto asm_path = std::string(TEST_RESOURCE_DIR) + "/" + file_name;
    auto cbc_path = std::string(TEST_RESOURCE_DIR) + "/" + file_name + ".obj";
    auto jar_path = std::string(TEST_RESOURCE_DIR) + "/cbc-asm.jar";

    auto command = "java -jar " + jar_path + ' ' + asm_path;
    auto file    = popen(command.c_str(), "r");
    pclose(file);
    return IO::OpenFile(std::filesystem::path(cbc_path));
}

struct Resolver : public API::Resolver {
    Resolver() {}

    API::Type* Resolve(Symlevel::Index<Symlevel::Terms::Term> index) override { return nullptr; }

    API::DirectMethod* ResolveDirectMethod(Symlevel::Index<Symlevel::MethodReference> index) override
    {
        return nullptr;
    }

    API::VirtualMethod* ResolveVirtualMethod(Symlevel::Index<Symlevel::MethodReference> index) override
    {
        return nullptr;
    }

    API::Field* Resolve(Symlevel::Index<Symlevel::FieldReference> index) override { return nullptr; }

    std::optional<API::Type*> TypeOf(Symlevel::Terms::Term* term) override { return std::nullopt; }

    ~Resolver() override = default;
};

std::unique_ptr<API::Resolver> MockResolver() { return std::make_unique<Resolver>(); }
