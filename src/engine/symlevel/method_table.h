#pragma once

#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/symlevel/definitions.h"
#include "engine/terms.h"
#include "utils/iterators.h"
#include "utils/logger.h"
#include <memory>
#include <vector>

namespace Symlevel { // TODO: move to Engine

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
    Engine::Term genericContext;

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
        Engine::Identifier<MethodDefinition> method;

        /// Declaring type, where method is actually declared. Additionally to type definition,
        /// stores an generic variable parameterization.
        Engine::Term genericContext;
    };

    struct SubTableGenerator {
        MethodTable const& table;
        std::vector<SubTable> const& subtables;
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

    void Globalize(Engine::Session& session);

    Range Classes() const;
    Range Interfaces() const;

    int ClassCount() const;
    int InterfaceCount() const;
    int EntryCount() const;

    struct Reference {
        std::string_view name;
        Term signature;
    };

    std::optional<MethodTableEntry> Resolve(Engine::Session& session, Reference const& reference) const;

    void ResolveAll(Engine::Session& session, Reference const& reference, std::vector<MethodTableEntry>& buffer) const;

private:
    friend class MethodSubTable;
    friend class MethodTableManager;

    MethodTable(
        std::vector<Entry>&& allEntries, std::vector<SubTable>&& classTables, std::vector<SubTable>&& interfaceTables
    );

    std::vector<Entry> allEntries;
    std::vector<SubTable> classTables;
    std::vector<SubTable> interfaceTables;
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
    static MethodTableManager& Of(Engine::Engine& engine);
    static MethodTableManager& Of(Engine::Session& session);

    static std::unique_ptr<MethodTableManager> NewInstance();

    virtual ~MethodTableManager() = default;

    /// Returns an method table for the given type definition.
    virtual std::optional<std::shared_ptr<MethodTable>> GetMethodTable(
        Engine::Session& session, Engine::Identifier<TypeDefinition> type
    ) = 0;

    /// Returns an method table for the given type.
    std::optional<std::shared_ptr<MethodTable>> GetMethodTable(Engine::Session& session, Engine::Term term);

protected:
    std::optional<MethodTable> BuildTable(Engine::Session& session, Engine::Identifier<TypeDefinition> type);
    MethodTable BaseTable();
};

namespace Log {
/// Logger for method table building and querying.
/// DEBUG - log method tables structure
/// INFO  - log queries of method table
/// ERROR - log errors
extern Logging::Logger mt;
} // namespace Log
} // namespace Symlevel
