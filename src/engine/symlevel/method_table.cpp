#include "method_table.h"
#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/reader.h"
#include "engine/symlevel/term.h"
#include "engine/terms.h"
#include "utils/iterators.h"
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace Symlevel {

using namespace Engine;

// ---- MethodTable ----

MethodTable::MethodTable(
    std::vector<Entry>&& allEntries, std::vector<SubTable>&& classTables, std::vector<SubTable>&& interfaceTables
)
    : allEntries(std::move(allEntries)),
      classTables(std::move(classTables)),
      interfaceTables(std::move(interfaceTables))
{}

MethodTable::Range MethodTable::Classes() const
{
    return Iterators::MakeRange(MethodTable::SubTableGenerator {
        .table     = *this,
        .subtables = classTables,
        .disp      = 0,
        .cursor    = 0,
    });
}

MethodTable::Range MethodTable::Interfaces() const
{
    return Iterators::MakeRange(MethodTable::SubTableGenerator {
        .table     = *this,
        .subtables = interfaceTables,
        .disp      = static_cast<int>(classTables.size()),
        .cursor    = 0,
    });
}

int MethodTable::ClassCount() const { return classTables.size(); }

int MethodTable::InterfaceCount() const { return interfaceTables.size(); }

int MethodTable::EntryCount() const { return allEntries.size(); }

void MethodTable::Globalize(Session& session)
{
    auto& termManager = TermManager::Of(session);
    for (auto& entry : allEntries) {
        entry.genericContext = termManager.Globalize(entry.genericContext);
    }
    for (auto& st : classTables) {
        st.genericContext = termManager.Globalize(st.genericContext);
    }
    for (auto& st : interfaceTables) {
        st.genericContext = termManager.Globalize(st.genericContext);
    }
}

static bool Compare(Session& session, MethodTable::Reference const& reference, MethodTableEntry const& entry)
{
    auto method = Symlevel::Reader::Read(session, entry.method);
    auto name   = Symlevel::Reader::Read(session, method.Name());

    if (name.compare(reference.name) != 0) {
        return false;
    }

    auto signature = TermManager::Resolve(session, method.Signature());
    return signature == reference.signature;
}

std::optional<MethodTableEntry> MethodTable::Resolve(Session& session, MethodTable::Reference const& reference) const
{
    for (auto st : Classes()) {
        for (auto entry : st.Entries()) {
            if (Compare(session, reference, entry)) {
                return entry;
            }
        }
    }
    for (auto st : Interfaces()) {
        for (auto entry : st.Entries()) {
            if (Compare(session, reference, entry)) {
                return entry;
            }
        }
    }
    return std::nullopt;
}

void MethodTable::ResolveAll(Session& session, Reference const& reference, std::vector<MethodTableEntry>& buffer) const
{
    for (auto st : Classes()) {
        for (auto entry : st.Entries()) {
            if (Compare(session, reference, entry)) {
                buffer.push_back(entry);
            }
        }
    }
    for (auto st : Interfaces()) {
        for (auto entry : st.Entries()) {
            if (Compare(session, reference, entry)) {
                buffer.push_back(entry);
            }
        }
    }
}

// ---- MethodTable::SubTableGenerator ----

std::optional<MethodSubTable> MethodTable::SubTableGenerator::operator()()
{
    if (cursor < subtables.size()) {
        auto cursor = this->cursor++;
        auto& st    = subtables[cursor];
        return MethodSubTable(table, st.genericContext, st.start, st.end, cursor + disp);
    } else {
        return std::nullopt;
    }
}

// ---- MethodSubTable ----
MethodSubTable::MethodSubTable(MethodTable const& table, Term declaringType, int start, int end, int num)
    : table(&table),
      declaringType(declaringType),
      start(start),
      end(end),
      num(num)
{}

int MethodSubTable::StartPos() const { return start; }

int MethodSubTable::EndPos() const { return end; }

int MethodSubTable::Num() const { return num; }

Term MethodSubTable::DeclaringType() const { return declaringType; }

MethodSubTable::Range MethodSubTable::Entries() const
{
    return Iterators::MakeRange(MethodSubTable::EntryGenerator {
        .st     = *this,
        .cursor = start,
    });
}

std::optional<MethodTableEntry> MethodSubTable::EntryGenerator::operator()()
{
    if (cursor < st.end) {
        auto cursor             = this->cursor++;
        auto entry              = st.table->allEntries[cursor];
        MethodTableEntry mEntry = {
            .method         = entry.method,
            .genericContext = entry.genericContext,
            .methodNum      = cursor - st.start,
            .subTableNum    = st.num,
            .flatMethodNum  = cursor,
        };
        return mEntry;
    } else {
        return std::nullopt;
    }
}

// ---- MethodTable building ----

MethodTable MethodTableManager::BuildTable(Session& session, Identifier<TypeDefinition> type)
{
    auto def       = TypeDefinition::Resolve(session, type);
    auto methodSeq = def.GetVirtualMethods();
    auto superType = TermManager::Resolve(session, def.GetSuperType());

    // TODO: make term with type variables
    auto thisType = Term::Definition(session, type);

    // copy table
    auto newTable      = *GetMethodTable(session, superType);
    auto oldEntryCount = newTable.EntryCount();

    std::vector<Identifier<MethodDefinition>> declaredMethods;
    methodSeq.Read(session, declaredMethods);

    std::vector<MethodTableEntry> entryBuffer;

    for (auto methodId : declaredMethods) {
        auto newEntry = MethodTable::Entry {
            .method         = methodId,
            .genericContext = thisType,
        };

        auto method = Reader::Read(session, methodId);
        MethodTable::Reference ref { .name      = Reader::Read(session, method.Name()),
                                     .signature = TermManager::Resolve(session, method.Signature()) };

        newTable.ResolveAll(session, ref, entryBuffer);
        if (entryBuffer.empty()) {
            newTable.allEntries.emplace_back(newEntry);
            continue;
        }
        for (auto& entry : entryBuffer) {
            newTable.allEntries[entry.flatMethodNum] = newEntry;
        }
        entryBuffer.clear();
    }

    auto newEntryCount = newTable.EntryCount();
    newTable.classTables.emplace_back(MethodTable::SubTable {
        .genericContext = thisType,
        .start          = oldEntryCount,
        .end            = newEntryCount,
    });

    return newTable;
}

MethodTable MethodTableManager::BaseTable()
{
    MethodTable mt;
    mt.classTables.push_back(MethodTable::SubTable {
        .genericContext = Term::Predefined(TermKind::NIL),
        .start          = 0,
        .end            = 0,
    });
    return mt;
}

std::shared_ptr<MethodTable> MethodTableManager::GetMethodTable(Session& session, Term term)
{
    if (term.GetKind() == TermKind::NIL) {
        static auto mt = std::make_shared<MethodTable>(std::move(BaseTable()));
        return mt;
    }
    auto type = TypeTermId(term).GetIdentifier();

    // FIXME: instantiate!
    return GetMethodTable(session, type);
}

/// Caching policy notice.
///
/// Each method table can be build from the ground-up without side effects, using simple procedure
/// that involves only read-only data from cbc files. So any caching of result
/// is not functionally required.
struct CachingMethodTableManager : public MethodTableManager {
    using Ident = Identifier<TypeDefinition>;
    std::unordered_map<Ident::Packed, std::shared_ptr<MethodTable>, Ident::Hasher> tables;

    /// Returns an method table for the given type definition.
    std::shared_ptr<MethodTable> GetMethodTable(Session& session, Identifier<TypeDefinition> type) override
    {
        auto& tables = this->tables;

        auto it = tables.find(type.Pack());
        if (it != tables.end()) {
            return it->second;
        }

        auto mt = MethodTableManager::BuildTable(session, type);
        mt.Globalize(session);

        auto res = std::make_shared<MethodTable>(std::move(mt));

        tables.insert({ type.Pack(), res });
        return res;
    }
};

struct LockedMethodTableManager : public MethodTableManager {
    CachingMethodTableManager delegate;
    std::mutex lock;

    std::shared_ptr<MethodTable> GetMethodTable(Session& session, Identifier<TypeDefinition> type) override
    {
        std::lock_guard guard(lock);
        return delegate.GetMethodTable(session, type);
    }
};

std::unique_ptr<MethodTableManager> MethodTableManager::NewInstance()
{
    return std::make_unique<LockedMethodTableManager>();
}

} // namespace Symlevel
