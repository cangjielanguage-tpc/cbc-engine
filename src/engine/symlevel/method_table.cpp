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

namespace Symlevel {

struct CacheEntry {
    Engine::Identifier<MethodDefinition> method;
    Term declaringType;

    CacheEntry(Engine::Identifier<MethodDefinition> method, Term declaringType)
        : method(method),
          declaringType(declaringType)
    {}
};

struct MethodSubTable::Impl {
    Impl(std::vector<CacheEntry>& allEntries, Term declaringType, int num, size_t start, size_t end)
        : allEntries(allEntries),
          declaringType(declaringType),
          subTableNum(num),
          start(start),
          end(end)
    {
        ASSERT(start <= end);
    }

    Impl(Impl const& impl) : Impl(impl.allEntries, impl.declaringType, impl.subTableNum, impl.start, impl.end) {}

    std::vector<CacheEntry>& allEntries;
    Term declaringType;
    size_t start;
    size_t end;
    int subTableNum;
};

struct MethodTable::Impl {
    std::vector<CacheEntry> allEntries;
    std::vector<MethodSubTable> classTables;
    std::vector<MethodSubTable> interfaceTables;
};

struct MethodTableManager::Impl {
    std::mutex lock;
    std::unordered_map<Engine::Identifier<TypeDefinition>, MethodTable> tables;
};

void MethodTable::Find(Engine::Session& session, String name, std::vector<MethodTableEntry>& candidates)
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

void MethodTable::ForEachClassSubTable(std::function<void(MethodSubTable&)> const& f)
{
    for (auto& t : impl->classTables)
        f(t);
}

void MethodTable::ForEachInterfaceSubTable(std::function<void(MethodSubTable&)> const& f)
{
    for (auto& t : impl->interfaceTables)
        f(t);
}

size_t MethodTable::ClassSubTableCount() { return impl->classTables.size(); }

size_t MethodTable::InterfaceSubTableCount() { return impl->interfaceTables.size(); }

MethodTable::~MethodTable() = default;

MethodTable::MethodTable(std::shared_ptr<Impl> impl) : impl(std::move(impl)) {}

MethodTable::MethodTable(MethodTable&& other)      = default;
MethodTable::MethodTable(MethodTable const& other) = default;

MethodSubTable::MethodSubTable(std::unique_ptr<Impl> impl) : impl(std::move(impl)) {}

MethodSubTable::MethodSubTable(MethodSubTable&& other) = default;
MethodSubTable::~MethodSubTable()                      = default;

Term MethodSubTable::DeclaringType() { return impl->declaringType; }

void MethodSubTable::ForEach(std::function<void(MethodTableEntry)> const& f)
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

size_t MethodSubTable::Size() { return impl->end - impl->start; }

MethodTableManager::MethodTableManager() : impl(std::make_unique<MethodTableManager::Impl>()) {}

MethodTableManager::MethodTableManager(MethodTableManager&& manager) : impl(std::move(manager.impl)) {}

MethodTableManager::~MethodTableManager() = default;

static std::shared_ptr<MethodTable::Impl> BuildTable(Engine::Session& session, Engine::Identifier<TypeDefinition> type)
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
    std::vector<CacheEntry> allEntries;
    for (auto offs : methods) {
        Engine::Identifier<MethodDefinition> def(offs, methodSeq.FileId());
        allEntries.emplace_back(def, declaringTypeTerm);
    }

    // table with only one class.
    auto table = std::make_shared<MethodTable::Impl>();
    auto classTable =
        std::make_unique<MethodSubTable::Impl>(table->allEntries, declaringTypeTerm, 0, 0, allEntries.size());

    table->classTables.emplace_back(std::move(classTable));

    return table;
}

MethodTable MethodTableManager::GetMethodTable(Engine::Session& session, Engine::Identifier<TypeDefinition> type)
{
    auto& manager = MethodTableManager::Of(session);
    std::lock_guard guard(manager.impl->lock);

    auto& tables = manager.impl->tables;

    auto it = tables.find(type);
    if (it != tables.end()) {
        return it->second;
    }

    MethodTable mt(std::move(BuildTable(session, type)));
    tables.insert({ type, mt });

    return mt;
}

} // namespace Symlevel
