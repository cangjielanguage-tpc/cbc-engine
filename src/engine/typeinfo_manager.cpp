#include "typeinfo_manager.h"
#include "engine/engine.h"
#include "engine/terms.h"
#include "runtimesupport/runtime.h"
#include "runtimesupport/typeinfo_factory.h"
#include "utils/assertion.h"
#include <mutex>
#include <optional>
#include <variant>

namespace Engine {

using TypeInfo = RTSupport::TypeInfo;

struct Failed {
} failed;

struct Pending {
} pending;

using ResolutionState = std::variant<TypeInfo, Failed, Pending>;

struct TermHasher {
    uint64_t operator()(GlobalTerm const& term) const { return term.Hash(); }
};

struct BasicTypeInfoManager : public TypeInfoManager {
    std::optional<RTSupport::TypeInfo> AcquireTypeInfo(Session& session, GlobalTerm term) override
    {
        auto it = storage.find(term);
        if (storage.end() != it) {
            auto state = it->second;
            if (std::holds_alternative<TypeInfo>(state)) {
                return std::get<TypeInfo>(state);
            } else if (std::holds_alternative<Failed>(state)) {
                return std::nullopt;
            } else if (std::holds_alternative<Pending>(state)) {
                // recursive access
                storage.insert_or_assign(term, failed);
                return std::nullopt;
            }
        }

        storage.insert({ term, Pending {} });

        auto result = RTSupport::CreateTypeInfo(session, *this, term);
        if (result.has_value()) {
            auto uuid = RTSupport::MetaInfo::GetUUID(result.value());
            if (auto it = uuidMap.find(uuid); it != uuidMap.end()) {
                // The `result` Typeinfo already been created before,
                // which is possible only if we incorrectly reconstructed term
                // from TypeInfo.
                FATAL("Unexpected UUID associated term.");
            }
            uuidMap.insert_or_assign(uuid, term);
            storage.insert_or_assign(term, result.value());
        } else {
            storage.insert_or_assign(term, failed);
        }

        return result;
    }

    GlobalTerm AcquireTerm(Session& session, RTSupport::TypeInfo ti) override
    {
        auto uuid = RTSupport::MetaInfo::GetUUID(ti);
        auto it = uuidMap.find(uuid);
        if (it != uuidMap.end()) {
            return it->second;
        }
        // unknown uuid -> aot type was provided
        // We need to accurately reconstruct term.
        auto term = RTSupport::ReconstructTerm(session, *this, ti);
        uuidMap.insert_or_assign(uuid, term);
        return term;
    }

protected:
    std::unordered_map<GlobalTerm, ResolutionState, TermHasher> storage;
    std::unordered_map<RTSupport::TypeInfoUUID, GlobalTerm> uuidMap;
};

struct LockedTypeInfoManager : public TypeInfoManager {
    std::optional<RTSupport::TypeInfo> AcquireTypeInfo(Session& session, GlobalTerm term) override
    {
        std::lock_guard guard(lock);
        return unsafe.AcquireTypeInfo(session, term);
    }

    GlobalTerm AcquireTerm(Session& session, RTSupport::TypeInfo ti) override
    {
        std::lock_guard guard(lock);
        return unsafe.AcquireTerm(session, ti);
    }

private:
    BasicTypeInfoManager unsafe;

    // Recursive mutexes are avoided intentionally.
    std::mutex lock;
};

std::unique_ptr<TypeInfoManager> TypeInfoManager::NewInstance() { return std::make_unique<LockedTypeInfoManager>(); }

std::optional<RTSupport::TypeInfo> TypeInfoManager::AcquireTypeInfo(Session& session, Term& term)
{
    auto gterm = TermManager::Of(session).Globalize(term);
    return AcquireTypeInfo(session, gterm);
}

} // namespace Engine
