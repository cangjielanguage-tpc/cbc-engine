#pragma once

#include <string>
#include <string_view>

#include "api/method.h"

namespace API {
namespace Fake {

class Method : public API::Method {
public:
    Term* ABISignature() override { return nullptr; }

    std::optional<Type*> RefType() override { return std::optional<Type*>(); }

    MethodFlags Flags() override { return MethodFlags(); }

    std::string_view FullName() override { return "fake_method"; }
};

} // namespace Fake
} // namespace API
