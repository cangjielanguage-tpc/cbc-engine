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

    Term DeclaringType();
    void ForEach(std::function<void(MethodTableEntry)> const& f);
    size_t Size();

    ~MethodSubTable() = default;

private:
    MethodSubTable(std::shared_ptr<Impl> impl);

    std::unique_ptr<Impl> impl;
};

class MethodTable {
public:
    class Impl;
    friend class Impl;
    // virtual MethodTable& Instantiate(targs) = 0;

    void Find(Engine::Session& session, String name, std::vector<MethodTableEntry>& candidates);
    void ForEachClassSubTable(std::function<void(MethodSubTable&)> const& f);
    void ForEachInterfaceSubTable(std::function<void(MethodSubTable&)> const& f);
    size_t ClassSubTableCount();
    size_t InterfaceSubTableCount();
    ~MethodTable();

private:
    MethodTable(std::shared_ptr<Impl> impl);

    std::shared_ptr<Impl> impl;
};

class MethodTableManager {
public:
    /// Returns an method table for the given type definition.
    MethodTable& GetMethodTable(Engine::Session& session, Engine::Identifier<TypeDefinition> type);
};

} // namespace Symlevel
