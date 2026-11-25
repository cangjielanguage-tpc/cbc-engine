#pragma once

#include "engine/engine.h"
#include "engine/terms.h"
#include "engine/typeinfo_manager.h"
#include "runtimesupport/runtime.h"
#include <optional>

namespace RTSupport {

struct TypeInfoManager : public Engine::TypeInfoManager {
    struct Fixup {
        Engine::GlobalTerm term;
        TypeInfo* location;
    };

    /// Register partially initialized typeInfo to allow recursive queries during typeinfo creation.
    virtual void RegisterPartial(Engine::GlobalTerm term, TypeInfo typeInfo) = 0;

private:
    Utils::Vector<Fixup> fixups;
};

/// Performs creation of type info for the given instance of term.
/// Note, that this method can use TypeInfoManager to query `TypeInfo` of other terms,
/// but do not queries `TypeInfo` for the term provided.
///
/// In case of resolution errors, nullopt is returned.
///
/// Note that TypeInfo creation can be used as part of `TypeInfoManager::AcquireTypeInfo` query,
/// which must be thread-safe.
/// To prevent deadlocks on recursive queries, safe implementation of `TypeInfoManager` is passed explicitly.
std::optional<TypeInfo> CreateTypeInfo(Engine::Session& session, TypeInfoManager& manager, Engine::GlobalTerm term);

/// Fills `Interpretation::builtinTypeInfos` with engine-built type infos of the primitive types.
/// Built the same way as the other engine-provided type infos (VArray, Tuple, ...), so it does
/// not query the AOT runtime and succeeds at any initialization point, even when no AOT library
/// is loaded yet.
///
/// Must be called once during engine initialization, before any emitter or interpreter run:
/// the emitter reads the array at emit time to bake type info pointers into bytecode.
void InitBuiltinTypeInfos();

/// Register loaded CBC Any/Object and initialize still-null native core-type caches.
/// Call after acquiring helper handles, before preparing bodies or running code.
void InitCoreTypeInfoMetadata(Engine::Session& session);

/// Attach CBC extension metadata to the early-allocated primitive type infos.
/// Call after loading all CBC images and acquiring helper handles, before preparing bodies.
void InitBuiltinTypeInfoMetadata(Engine::Session& session);

/// Reconstruct term based on TypeInfo being provided.
/// Expects that `ti` is not CBC provided!
Engine::GlobalTerm ReconstructTerm(Engine::Session& session, TypeInfoManager& manager, TypeInfo ti);

} // namespace RTSupport
