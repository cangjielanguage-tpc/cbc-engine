#include "method_table.h"
#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/terms.h"
#include "utils/assertion.h"
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

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
        auto it = t.Iter();
        while (it.HasNext()) {
            checkAndAdd(it.Next());
        }
    }

    for (auto& t : impl->interfaceTables) {
        auto it = t.Iter();
        while (it.HasNext()) {
            checkAndAdd(it.Next());
        }
    }
}

MethodTable::Iterator MethodTable::EntriesIter() { return Iterator(impl.get()); }

MethodTable::TableIterator MethodTable::ClassSubTableIter() { return TableIterator(impl->classTables); }

MethodTable::TableIterator MethodTable::InterfaceSubTableIter() { return TableIterator(impl->interfaceTables); }

bool MethodTable::TableIterator::HasNext() { return cursor < tables.size(); }

MethodSubTable const& MethodTable::TableIterator::Next()
{
    ASSERT(HasNext());
    return tables[cursor++];
}

bool MethodTable::Iterator::HasNext() { return cursor < table->allEntries.size(); }

Engine::Identifier<MethodDefinition> MethodTable::Iterator::Next()
{
    ASSERT(HasNext());
    return table->allEntries[cursor].method;
}

size_t MethodTable::ClassSubTableCount() const { return impl->classTables.size(); }

size_t MethodTable::InterfaceSubTableCount() const { return impl->interfaceTables.size(); }

size_t MethodTable::EntryCount() const { return impl->allEntries.size(); }

MethodTable::~MethodTable() = default;

MethodTable::MethodTable(std::shared_ptr<Impl> impl) : impl(std::move(impl)) {}

MethodTable::MethodTable(MethodTable&& other)      = default;
MethodTable::MethodTable(MethodTable const& other) = default;

MethodSubTable::MethodSubTable(std::unique_ptr<Impl> impl) : impl(std::move(impl)) {}

MethodSubTable::MethodSubTable(MethodSubTable&& other) = default;
MethodSubTable::~MethodSubTable()                      = default;

Term MethodSubTable::DeclaringType() const { return impl->declaringType; }

MethodSubTable::Iterator MethodSubTable::Iter() const { return Iterator(this->impl.get(), impl->start); }

bool MethodSubTable::Iterator::HasNext() { return cursor < table->end; }

MethodTableEntry MethodSubTable::Iterator::Next()
{
    ASSERT(HasNext());
    auto entry              = table->allEntries[cursor];
    MethodTableEntry mEntry = {
        .method        = entry.method,
        .declaringType = table->declaringType,
        .methodNum     = static_cast<int>(cursor - table->start),
        .subTableNum   = table->subTableNum,
    };
    cursor++;
    return mEntry;
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

    // table with only one class.
    auto table = std::make_shared<MethodTable::Impl>();

    // TODO: support hierarchy
    //       1. get tables for all super-types;
    //       2. appropriately instantiate tables;
    //       3. search for overrides;
    //       4. split overriden methods and newly declared methods;
    //       5. copy subtables of tables from super-types to new table;
    //       6. patch entries in copied subtables with overriden methods;
    //       7. append newly declared methods to `allEntries`;
    //       8. introduce new class/interface table with newly declared methods;
    for (auto offs : methods) {
        Engine::Identifier<MethodDefinition> def(offs, methodSeq.FileId());
        table->allEntries.emplace_back(def, declaringTypeTerm);
    }

    auto classTable =
        std::make_unique<MethodSubTable::Impl>(table->allEntries, declaringTypeTerm, 0, 0, table->allEntries.size());

    table->classTables.emplace_back(std::move(classTable));
    return MethodTable(table);
}

/// Caching policy notice.
///
/// Each method table can be build from the ground-up without side effects, using simple procedure
/// that involves only read-only data from cbc files. So any caching of result
/// is not functionally required.

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

MethodTable MethodTableManager::GetMethodTable(Engine::Session& session, Term term)
{
    auto ident    = term.GetIdentifier().AsTypeIdent();
    auto type     = Engine::Identifier<Symlevel::TypeDefinition>(ident.GetOffset(), ident.GetFile());
    auto& manager = Symlevel::MethodTableManager::Of(session);

    // FIXME: instantiate!
    return manager.GetMethodTable(session, type);
}

} // namespace Symlevel
