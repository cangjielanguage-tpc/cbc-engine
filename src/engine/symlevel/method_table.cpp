#include "method_table.h"
#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/terms.h"
#include "utils/assertion.h"
#include <cstddef>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

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
namespace Symlevel {

struct TableEntry {
    Engine::Identifier<MethodDefinition> method;
    Term declaringType;

    TableEntry(Engine::Identifier<MethodDefinition> method, Term declaringType)
        : method(method),
          declaringType(declaringType)
    {}
};

struct MethodSubTable::Impl {
    Impl(std::vector<TableEntry>& allEntries, Term declaringType, int num, size_t start, size_t end)
        : allEntries(allEntries),
          declaringType(declaringType),
          subTableNum(num),
          start(start),
          end(end)
    {
        ASSERT(start <= end);
    }

    Impl(Impl const& impl) : Impl(impl.allEntries, impl.declaringType, impl.subTableNum, impl.start, impl.end) {}

    std::vector<TableEntry>& allEntries;
    Term declaringType;
    size_t start;
    size_t end;
    int subTableNum;
};

struct MethodTable::Impl {
    std::vector<TableEntry> allEntries;
    std::vector<MethodSubTable> classTables;
    std::vector<MethodSubTable> interfaceTables;
};

void MethodTable::Find(Engine::Session& session, String name, std::vector<MethodTableEntry>& candidates) const
{
    auto checkAndAdd = [&name, &candidates, &session](MethodTableEntry entry) -> void {
        auto def        = MethodDefinition::Resolve(session, entry.method);
        auto methodName = String::Parse(session, def.FileId(), def.NameOffset());
        if (name.compare(methodName) == 0) {
            candidates.emplace_back(entry);
        }
    };

    for (auto& t : impl->classTables) {
        t.ForEach(checkAndAdd);
    }

    for (auto& t : impl->interfaceTables) {
        t.ForEach(checkAndAdd);
    }
}

void MethodTable::ForEachClassSubTable(std::function<void(MethodSubTable const&)> const& f) const
{
    for (auto& t : impl->classTables)
        f(t);
}

void MethodTable::ForEachInterfaceSubTable(std::function<void(MethodSubTable const&)> const& f) const
{
    for (auto& t : impl->interfaceTables)
        f(t);
}

size_t MethodTable::ClassSubTableCount() const { return impl->classTables.size(); }

size_t MethodTable::InterfaceSubTableCount() const { return impl->interfaceTables.size(); }

MethodTable::~MethodTable() = default;

MethodTable::MethodTable(std::shared_ptr<Impl> impl) : impl(std::move(impl)) {}

MethodTable::MethodTable(MethodTable&& other)      = default;
MethodTable::MethodTable(MethodTable const& other) = default;

MethodSubTable::MethodSubTable(std::unique_ptr<Impl> impl) : impl(std::move(impl)) {}

MethodSubTable::MethodSubTable(MethodSubTable&& other) = default;
MethodSubTable::~MethodSubTable()                      = default;

Term MethodSubTable::DeclaringType() const { return impl->declaringType; }

void MethodSubTable::ForEach(std::function<void(MethodTableEntry const)> const& f) const
{
    auto start         = impl->start;
    auto end           = impl->end;
    auto& allEntries   = impl->allEntries;
    auto declaringType = impl->declaringType;
    auto subTableNum   = impl->subTableNum;

    for (auto p = start; p < end; p++) {
        auto entry              = allEntries.at(p);
        MethodTableEntry mEntry = {
            .method        = entry.method,
            .declaringType = declaringType,
            .methodNum     = static_cast<int>(p - start),
            .subTableNum   = subTableNum,
        };
        f(mEntry);
    }
}

size_t MethodSubTable::Size() const { return impl->end - impl->start; }

static MethodTable BuildTable(Engine::Session& session, Engine::Identifier<TypeDefinition> type)
{
    auto def       = TypeDefinition::Resolve(session, type);
    auto methodSeq = def.GetVirtualMethods();

    std::vector<Offset<MethodDefinition>> methods;
    methodSeq.Read(session, methods);

    // TODO: fixup declaring type term if it is references aot type.
    // TODO: make term with type variables
    auto declaringTypeTerm = Term::Definition(session, type);
    declaringTypeTerm      = TermManager::Of(session).Globalize(declaringTypeTerm);

    // TODO: support hierarchy
    //       1. get tables for all super-types;
    //       2. appropriately instantiate tables;
    //       3. search for overrides;
    //       4. split overriden methods and newly declared methods;
    //       5. copy subtables of tables from super-types to new table;
    //       6. patch entries in copied subtables with overriden methods;
    //       7. append newly declared methods to `allEntries`;
    //       8. introduce new class/interface table with newly declared methods;
    std::vector<TableEntry> allEntries;
    for (auto offs : methods) {
        Engine::Identifier<MethodDefinition> def(offs, methodSeq.FileId());
        allEntries.emplace_back(def, declaringTypeTerm);
    }

    // table with only one class.
    auto table = std::make_shared<MethodTable::Impl>();
    auto classTable =
        std::make_unique<MethodSubTable::Impl>(table->allEntries, declaringTypeTerm, 0, 0, allEntries.size());

    table->classTables.emplace_back(std::move(classTable));
    return MethodTable(table);
}

MethodTable MethodTableManager::GetMethodTable(Engine::Session& session, Engine::Identifier<TypeDefinition> type)
{
    auto& manager = MethodTableManager::Of(session);
    std::lock_guard guard(manager.lock);

    auto& tables = manager.tables;

    auto it = tables.find(type);
    if (it != tables.end()) {
        return it->second;
    }

    MethodTable mt = BuildTable(session, type);
    tables.insert({ type, mt });

    return mt;
}

} // namespace Symlevel
