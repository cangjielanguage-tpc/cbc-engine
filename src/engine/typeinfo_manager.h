#pragma once

#include "engine/engine.h"
#include "engine/symlevel/terms.h"
#include "runtimesupport/runtime.h"
#include <memory>

namespace Engine {

/// This class provides access to TypeInfo instances of given terms.
/// The term can be either backed by CBC type definition, AOT type or some basic types
/// for some cases.
///
/// The manager is responsible both for storage of corresponding `TypeInfo` for given terms
/// and their construction/resolution.
struct TypeInfoManager {
    static TypeInfoManager& Of(Engine& engine);
    static TypeInfoManager& Of(Session& session);

    static std::unique_ptr<TypeInfoManager> NewInstance();

    virtual ~TypeInfoManager() = default;

    std::optional<RTSupport::TypeInfo> AcquireTypeInfo(Session& session, Symlevel::Term& term);
    virtual std::optional<RTSupport::TypeInfo> AcquireTypeInfo(Session& session, Symlevel::GlobalTerm term) = 0;
};

} // namespace Engine
