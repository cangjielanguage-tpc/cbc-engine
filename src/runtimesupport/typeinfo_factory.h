#pragma once

#include "engine/engine.h"
#include "engine/terms.h"
#include "engine/typeinfo_manager.h"
#include "runtimesupport/runtime.h"
#include <optional>

namespace RTSupport {

/// Performs creation of type info for the given instance of term.
/// Note, that this method can use TypeInfoManager to query `TypeInfo` of other terms,
/// but do not queries `TypeInfo` for the term provided.
///
/// In case of resolution errors, nullopt is returned.
///
/// Note that TypeInfo creation can be used as part of `TypeInfoManager::AcquireTypeInfo` query,
/// which must be thread-safe.
/// To prevent deadlocks on recursive queries, safe implementation of `TypeInfoManager` is passed explicitly.
std::optional<TypeInfo> CreateTypeInfo(
    Engine::Session& session, Engine::TypeInfoManager& manager, Engine::GlobalTerm term
);

} // namespace RTSupport
