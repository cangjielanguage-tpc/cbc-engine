#pragma once

#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/string.h"
#include "engine/symlevel/terms.h"
#include <cstddef>
#include <functional>
#include <memory>
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

    Term DeclaringType();
    void ForEach(std::function<void(MethodTableEntry)> const& f);
    size_t Size();

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
    // virtual MethodTable& Instantiate(targs) = 0;

    void Find(Engine::Session& session, String name, std::vector<MethodTableEntry>& candidates);
    void ForEachClassSubTable(std::function<void(MethodSubTable&)> const& f);
    void ForEachInterfaceSubTable(std::function<void(MethodSubTable&)> const& f);
    size_t ClassSubTableCount();
    size_t InterfaceSubTableCount();
    ~MethodTable();

private:
    friend class MethodTableManager;
    std::shared_ptr<Impl> impl;
};

class MethodTableManager {
public:
    static MethodTableManager& Of(Engine::Engine& engine);
    static MethodTableManager& Of(Engine::Session& session);

    MethodTableManager();
    MethodTableManager(MethodTableManager&& manager);
    ~MethodTableManager();

    /// Returns an method table for the given type definition.
    MethodTable GetMethodTable(Engine::Session& session, Engine::Identifier<TypeDefinition> type);

private:
    class Impl;
    friend class Impl;

    std::unique_ptr<Impl> impl;
};

} // namespace Symlevel
