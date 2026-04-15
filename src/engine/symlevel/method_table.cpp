#include "method_table.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/terms.h"
#include <cstddef>
#include <vector>

namespace Symlevel {

struct Entry {
    Engine::Identifier<MethodDefinition> method;
    Term declaringType;
};

struct MethodSubTable::Impl {
    Impl(std::vector<Entry>& allEntries, Term declaringType, int num, size_t start, size_t end)
        : allEntries(allEntries),
          declaringType(declaringType),
          subTableNum(num),
          start(start),
          end(end)
    {}

    int subTableNum;
    Term declaringType;
    size_t start;
    size_t end;
    std::vector<Entry>& allEntries;
};

struct MethodTable::Impl {
    std::vector<Entry> allEntries;
    std::vector<MethodSubTable> classTables;
    std::vector<MethodSubTable> interfaceTables;
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

} // namespace Symlevel
