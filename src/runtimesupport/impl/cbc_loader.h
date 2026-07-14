#pragma once

#include <string>

namespace Engine {
class Loader;
}

namespace RTSupport {

void LoadCbcFilesFromDirectory(Engine::Loader& loader, std::string const& cbcDir);

void LoadCbcFilesFromDirectory(Engine::Loader& loader, std::string const& cbcDir, std::string const& excludedCbc);

} // namespace RTSupport
