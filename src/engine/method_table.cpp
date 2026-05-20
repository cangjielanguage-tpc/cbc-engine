#include "method_table.h"
#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/resolving_output.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/reader.h"
#include "engine/symlevel/term.h"
#include "engine/terms.h"
#include "utils/iterators.h"
#include "utils/logger.h"
#include "utils/ostream.h"
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace Engine {

using namespace Stream;
using namespace Symlevel;

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

std::optional<MethodTable> MethodTableManager::BuildTable(Session& session, Identifier<TypeDefinition> type)
{
    auto def   = Reader::Read(session, type);
    auto flags = def.GetFlags();

    // 1. Get table of super type for claseses or empty table for other types
    MethodTable newTable {};

    ClassSubstitution substitute(session, Term::Definition(session, type));

    if (flags.Is(TypeKind::CLASS)) {
        auto superType = TermManager::Resolve(session, def.GetSuperType());
        superType      = substitute(superType);

        auto superMT = GetMethodTable(session, superType);
        if (!superMT.has_value()) {
            ResolvingOutput out(session, Log::mt.Stream(Logging::Level::ERROR));
            out << "Super " << def.GetSuperType() << " of type " << Detailed(def.GetName()) << " not found." << endl;
            return std::nullopt;
        }
        newTable = **superMT;
    }

    // 2. Copy all entries and sub tables of interfaces, adjusting their views
    for (auto interf : def.GetInterfaces().Values(session)) {
        auto interface = TermManager::Resolve(session, interf);
        interface      = substitute(interface);

        auto optInterfTable = GetMethodTable(session, interface);
        if (!optInterfTable.has_value()) {
            ResolvingOutput out(session, Log::mt.Stream(Logging::Level::ERROR));
            out << "Interface " << interf << " of type " << Detailed(def.GetName()) << " not found." << endl;
            return std::nullopt;
        }

        auto interfTable = *optInterfTable;

        ASSERTION(interfTable->classTables.empty(), "interface tables should not have class table");

        auto oldEntryCount = newTable.EntryCount();
        newTable.allEntries.insert(
            newTable.allEntries.end(), interfTable->allEntries.begin(), interfTable->allEntries.end()
        );

        for (auto st : interfTable->interfaceTables) {
            newTable.interfaceTables.emplace_back(MethodTable::SubTable {
                .genericContext = st.genericContext,
                .start          = st.start + oldEntryCount,
                .end            = st.end + oldEntryCount,
            });
        }
    }

    Log::mt.Log(Logging::Level::DEBUG, [&session, &newTable, &def](Output& stream) {
        ResolvingOutput out(session, Log::mt.Stream(Logging::Level::ERROR));
        out << "Intermediate table for " << Detailed(def.GetName()) << " " << newTable << endl;
    });

    // 3. Patch all overriden methods and add newly declared methods
    // to the subtable of current type.
    // TODO: make term with type variables
    auto thisType      = Term::Definition(session, type);
    auto oldEntryCount = newTable.EntryCount();

    std::vector<MethodTableEntry> entryBuffer;
    for (auto methodId : def.GetVirtualMethods().Values(session)) {
        auto newEntry = MethodTable::Entry {
            .method         = methodId,
            .genericContext = thisType,
        };

        auto method = Reader::Read(session, methodId);
        MethodTable::Reference ref { .name      = Reader::Read(session, method.Name()),
                                     .signature = TermManager::Resolve(session, method.Signature()) };

        // TODO: Do not override protected methods that are not visible from the current type.
        newTable.ResolveAll(session, ref, entryBuffer);

        if (entryBuffer.empty()) {
            // 3.2 add newly declared methods
            newTable.allEntries.emplace_back(newEntry);
        } else {
            // 3.1 patch overriden methods
            for (auto& entry : entryBuffer) {
                newTable.allEntries[entry.flatMethodNum] = newEntry;
            }
            entryBuffer.clear();
        }
    }

    // 3.3 add new subtable for current type (even if new methods were not added)
    auto tables = flags.Is(TypeKind::INTERFACE) ? &newTable.interfaceTables : &newTable.classTables;

    tables->emplace_back(MethodTable::SubTable {
        .genericContext = thisType,
        .start          = oldEntryCount,
        .end            = newTable.EntryCount(),
    });

    return newTable;
}

std::optional<std::shared_ptr<MethodTable>> MethodTableManager::GetMethodTable(Session& session, Term term)
{
    Log::mt.Log(Logging::Level::INFO, [&](Output& stream) {
        ResolvingOutput out(session, stream);
        out << "Requesting of method table for " << term << endl;
    });

    std::optional<std::shared_ptr<MethodTable>> result = std::nullopt;

    if (term.GetKind() == TermKind::NIL) {
        static auto mt = std::make_shared<MethodTable>();
        result         = std::atomic_load(&mt);
    } else if (term.GetKind() == TermKind::UNDEFINED) {
        result = std::nullopt;
    } else {
        auto type = TypeTermId(term).GetIdentifier();
        auto table = GetMethodTable(session, type);
        if (!table.has_value()) {
            result = std::nullopt;
        } else {
            ClassSubstitution substitute(session, term);
            MethodTable t = **table;

            for (auto& e : t.allEntries) {
                e.genericContext = substitute(e.genericContext);
            }

            for (auto& st : t.classTables) {
                st.genericContext = substitute(st.genericContext);
            }

            for (auto& st : t.interfaceTables) {
                st.genericContext = substitute(st.genericContext);
            }

            result = std::make_shared<MethodTable>(std::move(t));
        }
    }

    Log::mt.Log(Logging::Level::DEBUG, [&](Output& stream) {
        ResolvingOutput out(session, stream);
        if (result.has_value()) {
            out << term << " " << **result << endl;
        } else {
            out << "MT for " << term << " not built." << endl;
        }
    });

    return result;
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
    std::optional<std::shared_ptr<MethodTable>> GetMethodTable(Session& session, Identifier<TypeDefinition> type)
        override
    {
        auto& tables = this->tables;

        auto it = tables.find(type.Pack());
        if (it != tables.end()) {
            return it->second;
        }

        auto optMT = MethodTableManager::BuildTable(session, type);
        if (!optMT.has_value()) {
            return std::nullopt;
        }

        auto mt = *optMT;

        mt.Globalize(session);

        auto res = std::make_shared<MethodTable>(std::move(mt));

        tables.insert({ type.Pack(), res });
        return res;
    }
};

struct LockedMethodTableManager : public MethodTableManager {
    CachingMethodTableManager delegate;
    std::mutex lock;

    std::optional<std::shared_ptr<MethodTable>> GetMethodTable(Session& session, Identifier<TypeDefinition> type)
        override
    {
        std::lock_guard guard(lock);
        return delegate.GetMethodTable(session, type);
    }
};

std::unique_ptr<MethodTableManager> MethodTableManager::NewInstance()
{
    return std::make_unique<LockedMethodTableManager>();
}

static Descripted stream(cerr, "[MT] ");
Logging::Logger Log::mt(&stream, Logging::Level::ERROR);

} // namespace Engine
