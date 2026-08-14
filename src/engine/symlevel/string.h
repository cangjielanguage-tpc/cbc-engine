#pragma once

#include "io/file_id.h"
#include "offset.h"
#include <string>

namespace Symlevel {

class String : public std::string_view {
public:
    String(std::string_view view) : std::string_view(std::move(view)) {}
};

}; // namespace Symlevel
