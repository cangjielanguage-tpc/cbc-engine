#pragma once

#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/string.h"
#include "engine/symlevel/terms.h"
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace Symlevel {

// TODO: doc about two-level method table structure

struct MethodTableEntry {
    Engine::Identifier<MethodDefinition> method;
    Term declaringType;
    int methodNum;
    int subTableNum;
};

class MethodSubTable {
public:
    class Impl;
    friend class Impl;

    MethodSubTable(std::unique_ptr<Impl> impl);
    MethodSubTable(MethodSubTable&& other);

    Term DeclaringType() const;
    void ForEach(std::function<void(MethodTableEntry const)> const& f) const;
    size_t Size() const;

    ~MethodSubTable();

private:
    std::unique_ptr<Impl> impl;
};

class MethodTable {
public:
    class Impl;
    friend class Impl;

    MethodTable(std::shared_ptr<Impl> impl);
    MethodTable(MethodTable&& other);
    MethodTable(MethodTable const& other);

    void Find(Engine::Session& session, String name, std::vector<MethodTableEntry>& candidates) const;
    void ForEachClassSubTable(std::function<void(MethodSubTable const&)> const& f) const;
    void ForEachInterfaceSubTable(std::function<void(MethodSubTable const&)> const& f) const;
    size_t ClassSubTableCount() const;
    size_t InterfaceSubTableCount() const;
    ~MethodTable();

private:
    friend class MethodTableManager;
    std::shared_ptr<Impl> impl;
};

class MethodTableManager {
public:
    static MethodTableManager& Of(Engine::Engine& engine);
    static MethodTableManager& Of(Engine::Session& session);

    /// Returns an method table for the given type definition.
    MethodTable GetMethodTable(Engine::Session& session, Engine::Identifier<TypeDefinition> type);

    /// Returns an method table for the given type.
    // MethodTable GetMethodTable(Engine::Session& session, Term term);

private:
    std::mutex lock;
    std::unordered_map<Engine::Identifier<TypeDefinition>, MethodTable> tables;
};

} // namespace Symlevel
