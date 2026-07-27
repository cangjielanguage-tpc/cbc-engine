#pragma once

#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/symlevel/definitions.h"
#include "engine/terms.h"
#include "utils/iterators.h"
#include "utils/logger.h"
#include "utils/vector.h"
#include <memory>

namespace Engine {

/// Each method table can be constructed for some type definition or term that instantiates type definition.
/// The method table is needed for virtual and interface method resolution (including dynamic "static" methods).
///
/// The table consists of two layers, so any entry could be referenced by two indexes or actual type + method index.
/// To perform a method reference resolution of form `(ref type, method name, signature)` we need to:
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
/// `J`, where entries for overridden methods are patched. New methods would be added as new class sub table.

/// The value of that describe an entry in method table.
struct MethodTableEntry {
    /// The method which is being referenced.
    Identifier<Symlevel::MethodDefinition> method;

    /// Declaring type, where method is actually declared. Additionally to type definition,
    /// stores an generic variable parameterization.
    Term genericContext;

    /// Method number in sub table.
    int methodNum;

    /// Number of sub table.
    int subTableNum;

    /// idx in all entries array
    int flatMethodNum;
};

class MethodSubTable;

/// @see description on top of current header.
class MethodTable {
    struct SubTable {
        Term genericContext;
        int start;
        int end;
    };

public:
    MethodTable() = default;

    struct Entry {
        /// The method which is being referenced.
        Identifier<Symlevel::MethodDefinition> method;

        /// Declaring type, where method is actually declared. Additionally to type definition,
        /// stores an generic variable parameterization.
        Term genericContext;
    };

    struct SubTableGenerator {
        MethodTable const& table;
        Utils::Vector<SubTable> const& subtables;
        int const disp;
        int cursor;

        std::optional<MethodSubTable> operator()();
    };

    using Range = Iterators::SimpleRange<SubTableGenerator>;

    auto Entries()
    {
        struct EntryView {
            MethodTable const& table;

            auto begin() { return table.allEntries.begin(); }

            auto end() { return table.allEntries.end(); }
        };

        return EntryView { *this };
    };

    void Globalize(Session& session);

    Range Classes() const;
    Range Interfaces() const;

    int ClassCount() const;
    int InterfaceCount() const;
    int EntryCount() const;

    struct Reference {
        std::string_view name;
        Term signature;
    };

    /// Resolve the reference passed in hierarchy, e.g.:
    /// interfaces:
    ///   I: foo(0), bar(1), baz(2)
    ///   J <: I: qwe(0)
    /// class Foo <: I & J {}
    ///
    /// reference:
    /// - J.foo will be resolved up to (I, 0)
    /// - J.baz will be resolved up to (I, 2)
    /// - J.qwe will be resolved up to (J, 0)
    std::optional<MethodTableEntry> Resolve(Session& session, Reference const& reference) const;

    void ResolveAll(Session& session, Reference const& reference, Utils::Vector<MethodTableEntry>& buffer) const;

private:
    friend class MethodSubTable;
    friend class MethodTableManager;

    MethodTable(
        Utils::Vector<Entry>&& allEntries,
        Utils::Vector<SubTable>&& classTables,
        Utils::Vector<SubTable>&& interfaceTables
    );

    Utils::Vector<Entry> allEntries;
    Utils::Vector<SubTable> classTables;
    Utils::Vector<SubTable> interfaceTables;
};

/// Second layer of the table. Can query entries in this sub table and type that corresponds to
/// one of a super types of the owner of whole method table.
class MethodSubTable {
public:
    struct EntryGenerator {
        MethodSubTable const& st;
        int cursor;

        std::optional<MethodTableEntry> operator()();
    };

    using Range = Iterators::SimpleRange<EntryGenerator>;

    int StartPos() const;
    int EndPos() const;
    int Size() const;
    int Num() const;
    Term DeclaringType() const;
    Range Entries() const;

private:
    friend class MethodTable::SubTableGenerator;
    friend class MethodTableManager;

    MethodSubTable(MethodTable const& table, Term declaringType, int start, int end, int num);

    MethodTable const* table;
    Term declaringType;
    int start;
    int end;
    int num;
};

class MethodTableManager {
public:
    static MethodTableManager& Of(Engine& engine);
    static MethodTableManager& Of(Session& session);

    static std::unique_ptr<MethodTableManager> NewInstance();

    virtual ~MethodTableManager() = default;

    /// Returns an method table for the given type.
    virtual std::optional<std::shared_ptr<MethodTable>> GetMethodTableCached(Session& session, GlobalTerm term) = 0;

    /// Returns an method table for the given type.
    std::optional<std::shared_ptr<MethodTable>> GetMethodTable(Session& session, Term term);

protected:
    std::optional<MethodTable> BuildTable(Session& session, GlobalTerm type);
    MethodTable BaseTable();
};

namespace Log {
/// Logger for method table building and querying.
/// DEBUG - log method tables structure
/// INFO  - log queries of method table
/// ERROR - log errors
extern Logging::Logger mt;
} // namespace Log
} // namespace Engine
