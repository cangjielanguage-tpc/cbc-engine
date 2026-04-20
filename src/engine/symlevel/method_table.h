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

/// Each method table can be constructed for some type definition or term that instantiates type definition.
/// The method table is needed for virtual and interface method resolution (including dynamic "static" methods).
///
/// The table consists of two layers, so any entry could be referenced by two indexes or actual type + method index.
/// To perform an method reference resolution of form `(ref type, method name, signature)` we need to:
/// 1. Find a sub-table that corresponds to `ref type`;
/// 2. Find entry that corresponds to `method name; signature` (can require generic instantiation).
/// The entry found is resolution result.
///
/// The table is structured as an array of all entries, where each sub table is a view to the array,
/// so intersections are allowed.
/// This layout can help to abstract away an actual data needed in run time to perform dynamic call.
/// E.g. if VMT is structured as:
/// - flat array of methods
/// - mapping: interface type info -> offset in the flat array
/// so virtual methods could be referenced by one number, we can map our method table to this kind of layout easily.
///
/// A new table of class `A <: C & I & J` will look like as table for `C` with added interface sub tables from `I` and
/// `J`, where entries for overridden methods are patched. New methods would be addede as new class sub table.

/// The value of that describe an entry in method table.
struct MethodTableEntry {
    /// The method which is being referenced.
    Engine::Identifier<MethodDefinition> method;

    /// Declaring type, where method is actually declared. Additionally to type definition,
    /// stores an generic variable parameterization.
    Term declaringType;

    /// Method number in sub table.
    int methodNum;

    /// Number of sub table.
    int subTableNum;

    // int flatMethodNum;
};

/// Second layer of the table. Can query entries in this sub table and type that corresponds to
/// one of a super types of the owner of whole method table.
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

/// @see description on top of current header.
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
    MethodTable GetMethodTable(Engine::Session& session, Term term);

private:
    std::mutex lock;
    std::unordered_map<Engine::Identifier<TypeDefinition>, MethodTable> tables;
};

} // namespace Symlevel
