#include "method_table.h"
#include "engine/decode/reader.h"
#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/image/cbc_file.h"
#include "engine/image/flags.h"
#include "engine/resolving_output.h"
#include "engine/terms.h"
#include "utils/assertion.h"
#include "utils/iterators.h"
#include "utils/logger.h"
#include "utils/ostream.h"
#include "utils/span.h"
#include "utils/vector.h"
#include <cstdio>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace Engine {

using namespace Stream;
using namespace Image;

// ---- MethodTable ----

MethodTable::MethodTable(
    Utils::Vector<Entry>&& allEntries, Utils::Vector<SubTable>&& classTables, Utils::Vector<SubTable>&& interfaceTables
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
        .disp      = static_cast<int>(classTables.Size()),
        .cursor    = 0,
    });
}

int MethodTable::ClassCount() const { return classTables.Size(); }

int MethodTable::InterfaceCount() const { return interfaceTables.Size(); }

int MethodTable::EntryCount() const { return allEntries.Size(); }

void MethodTable::Globalize(Session& session)
{
    auto& termManager = TermManager::Of(session);
    for (auto& entry : allEntries) {
        for (auto& t : entry.genericContext) {
            t = termManager.Globalize(t);
        }
    }
    for (auto& st : classTables) {
        st.declaringType = termManager.Globalize(st.declaringType);
    }
    for (auto& st : interfaceTables) {
        st.declaringType = termManager.Globalize(st.declaringType);
    }
}

static bool Compare(Session& session, MethodTable::Reference const& reference, MethodTableEntry const& entry)
{
    auto method = Decode::Read(session, entry.method);
    auto name   = Decode::Read(session, method.Name());

    if (name.compare(reference.name) != 0) {
        return false;
    }

    MethodSignatureSubstitution sub(session, entry.genericContext);
    auto signature = TermManager::Resolve(session, method.Signature());
    signature = sub.Substitute(signature);
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

void MethodTable::ResolveAll(Session& session, Reference const& reference, Utils::Vector<MethodTableEntry>& buffer)
    const
{
    for (auto st : Classes()) {
        for (auto entry : st.Entries()) {
            if (Compare(session, reference, entry)) {
                buffer.PushBack(entry);
            }
        }
    }
    for (auto st : Interfaces()) {
        for (auto entry : st.Entries()) {
            if (Compare(session, reference, entry)) {
                buffer.PushBack(entry);
            }
        }
    }
}

// ---- MethodTable::SubTableGenerator ----

std::optional<MethodSubTable> MethodTable::SubTableGenerator::operator()()
{
    if (cursor < subtables.Size()) {
        auto cursor = this->cursor++;
        auto& st    = subtables[cursor];
        return MethodSubTable(table, st.declaringType, st.start, st.end, cursor + disp);
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

struct MethodTableBuilder {
    MethodTableManager& manager;
    GlobalTerm tableOwner;
    MethodTable table {};

    Utils::Vector<MethodTableEntry> entryBuffer;
    std::unordered_set<Term, Term::Hasher> interfaces;

    using MethodReference = MethodTable::Reference;

    struct MRefTraits {
        size_t operator()(MethodReference const& ref) const
        {
            std::hash<std::string_view> hstr;
            return hstr(ref.name) + ref.signature.Hash();
        }

        bool operator()(MethodReference const& a, MethodReference const& b) const
        {
            return a.name == b.name && a.signature == b.signature;
        }
    };

    struct MethodSymbol {
        MethodReference ref;
        MethodTable::Entry* mtEntry;
        bool isOverride;
        bool isAbstract;
        bool isNewEntry;
    };

    bool AddInterface(Session& session, Term interface)
    {
        if (interfaces.find(interface) != interfaces.end()) {
            // In case if interfaces were mentioned for second time in extension
            return true;
        }
        auto optInterfTable = manager.GetMethodTable(session, interface);
        if (!optInterfTable.has_value()) {
            LOGS_ERROR(Log::mt, session, "Interface {} not found", interface);
            return false;
        }

        auto interfTable = *optInterfTable;

        ASSERTION(interfTable->classTables.Empty(), "interface tables should not have class table");

        auto oldEntryCount = table.EntryCount();
        for (auto& e : interfTable->allEntries) {
            table.allEntries.PushBack(e);
        }

        for (auto st : interfTable->interfaceTables) {
            table.interfaceTables.EmplaceBack(MethodTable::SubTable {
                .declaringType = st.declaringType,
                .start          = st.start + oldEntryCount,
                .end            = st.end + oldEntryCount,
            });
        }
        interfaces.insert(interface);
        return true;
    }

    bool AddExtensions(Session& session)
    {
        Utils::Vector<Term> storage;
        TermMatcher matcher;

        auto& arena = session.Allocator();

        for (auto file : session.GetEngine().Files()) {
            for (auto extId : Image::Reader::Resolve(session, file.extensions)) {
                auto ext    = Image::Reader::Read(session, extId);
                auto prefix = TermManager::Resolve(session, ext.GetExtendedType());
                bool isPrefix = matcher.IsPrefix(prefix, tableOwner);
                bool completeMatch = isPrefix && !matcher.HasErrors();

                LOGS_DEBUG(Log::mt, session, "Matching {} against {} (prefix={}, complete={})", prefix, tableOwner, isPrefix, completeMatch);

                if (completeMatch) {
                    ASSERTION(
                        matcher.vars.Size() == ext->arity,
                        "Language constraint: all variables in `extend` should be used in the extended type"
                    );

                    ClassSubstitution sub(session, matcher.vars);
                    for (auto interf : Reader::Resolve(session, ext.GetInterfaces())) {
                        auto interface = TermManager::Resolve(session, interf);
                        interface      = sub(interface);

                        if (!AddInterface(session, interface)) {
                            return false;
                        }
                    }

                    for (auto methodId : Reader::Resolve(session, ext.GetVirtualMethods())) {
                        auto terms = Utils::Span<Term>(matcher.vars.Data(), matcher.vars.Size());
                        AddMethod(session, methodId, arena.Copy(terms));
                    }
                }
                matcher.Clear();
            }
        }
        return true;
    }

    void AddMethod(
        Session& session, Identifier<MethodDefinition> methodId, Utils::Span<Term> genericContext
    )
    {
        auto newEntry = MethodTable::Entry {
            .method         = methodId,
            .genericContext = genericContext,
        };

        auto method = Reader::Read(session, methodId);
        auto methodSig = TermManager::Resolve(session, method.Signature());
        MethodSignatureSubstitution sub(session, genericContext);
        methodSig      = sub(methodSig);

        MethodTable::Reference ref { .name      = Reader::Read(session, method.Name()),
                                     .signature = methodSig };

        // TODO: Do not override protected methods that are not visible from the current type.
        table.ResolveAll(session, ref, entryBuffer);

        if (entryBuffer.Empty()) {
            // 3.2 add newly declared methods
            table.allEntries.EmplaceBack(newEntry);
        } else {
            // 3.1 patch overriden methods
            for (auto& entry : entryBuffer) {
                table.allEntries[entry.flatMethodNum] = newEntry;
            }
            entryBuffer.Clear();
        }
    }

    bool VerifyAndResolveConflicts(Session& session, size_t newEntriesStartIdx, size_t currentClassEntriesStartIdx)
    {
        Utils::Vector<MethodSymbol> symbols;
        symbols.Reserve(table.allEntries.Size());

        auto* newEntriesStart = &table.allEntries[newEntriesStartIdx];
        auto* currentClassEntriesStart = &table.allEntries[currentClassEntriesStartIdx];

        for (auto& entry : table.allEntries) {
            auto method = Reader::Read(session, entry.method);
            auto methodSig = TermManager::Resolve(session, method.Signature());
            MethodSignatureSubstitution sub(session, entry.genericContext);
            methodSig      = sub(methodSig);
            MethodReference ref { .name      = Reader::Read(session, method.Name()),
                                  .signature = methodSig };

            bool isOverride = &entry >= currentClassEntriesStart;
            bool isNewEntry = &entry >= newEntriesStart;
            bool isAbstract = method.GetFlags().Is(MethodFlag::ABSTRACT);

            MethodSymbol desc {
                .ref = ref,
                .mtEntry = &entry,
                .isOverride = isOverride,
                .isAbstract = isAbstract,
                .isNewEntry = isNewEntry,
            };

            symbols.PushBack(desc);
        }

        std::unordered_map<MethodReference, std::vector<MethodSymbol*>, MRefTraits, MRefTraits> refUses;
        for (auto& sym : symbols) {
            refUses[sym.ref].push_back(&sym);
        }

        bool conflictFound = false;

        for (auto& [ref, syms] : refUses) {
            if (syms.size() <= 1) {
                // safe usage
                continue;
            }
            // possible conflict
            auto refName = ref.name;
            auto refSig = ref.signature;

            MethodSymbol* override = nullptr;
            MethodSymbol* oldNonAbstract = nullptr;
            MethodSymbol* nonAbstract = nullptr;
            int nonAbstractCount = 0;
            for (auto& sym : syms) {
                if (sym->isOverride) {
                    override = sym;
                }
                if (!sym->isAbstract) {
                    nonAbstract = sym;
                    nonAbstractCount++;
                }
                if (!sym->isAbstract && !sym->isNewEntry) {
                    oldNonAbstract = sym;
                }
            }

            // if there is override or old non-abstract symbol, conflict is not possible
            if (override || oldNonAbstract || (nonAbstractCount == 1)) {
                MethodSymbol *choice = override;
                choice = choice ? choice : oldNonAbstract;
                choice = choice ? choice : nonAbstract;

                // no conflicts
                for (auto& sym : syms) {
                    *sym->mtEntry = *choice->mtEntry;
                }
                continue;
            }
            // no override, no old non-abstract symbols
            // if two non-abstract sybols found => conflict found
            if (nonAbstractCount <= 1) {
                continue;
            }
            // conflict is possibly present, final corner case:
            // "the same method could present in different entries method"
            auto refMethod = nonAbstract->mtEntry->method;

            for (auto& sym : syms) {
                if (!sym->isAbstract && nonAbstract != sym && nonAbstract->mtEntry->method != sym->mtEntry->method) {
                    LOGS_ERROR(Log::mt, session, "Conflict found for reference {}{} in {} with {}",
                            refName, refSig, Detailed(refMethod), Detailed(sym->mtEntry->method));
                    conflictFound = true;
                }
            }
        }
        return conflictFound;
    }
};

std::optional<MethodTable> MethodTableManager::BuildTable(Session& session, GlobalTerm type)
{
    auto def   = Reader::Read(session, ExtractTypeDefIdentifier(type));
    auto flags = def.GetFlags();

    // 1. Get table of super type for claseses or empty table for other types
    MethodTableBuilder builder { *this, type };

    ClassSubstitution substitute(session, type);

    if (flags.Is(TypeKind::CLASS)) {
        auto superType = TermManager::Resolve(session, def.GetSuperType());
        superType      = substitute(superType);

        auto superMT = GetMethodTable(session, superType);
        if (!superMT.has_value()) {
            ResolvingOutput out(session, Log::mt.Stream(Logging::Level::ERROR));
            out << "Super " << def.GetSuperType() << " of type " << Detailed(def.GetName()) << " not found." << endl;
            return std::nullopt;
        }
        builder.table = **superMT;
    }

    int oldEntryCount = builder.table.EntryCount();

    // 2. Copy all entries and sub tables of interfaces, adjusting their views
    for (auto interf : Reader::Resolve(session, def.GetInterfaces())) {
        auto interface = TermManager::Resolve(session, interf);
        interface      = substitute(interface);

        if (!builder.AddInterface(session, interface)) {
            return std::nullopt;
        }
    }

    if (!builder.AddExtensions(session)) {
        return std::nullopt;
    }

    LOGS_DEBUG(Log::mt, session, "Intermediate table for {} {}", Detailed(def.GetName()), builder.table);

    // 3. Patch all overriden methods and add newly declared methods
    // to the subtable of current type.
    auto thisType      = type;
    auto entryCountWithInterfaces = builder.table.EntryCount();
    MethodSignatureSubstitution methodSigSub(session, type);

    Utils::Vector<MethodTableEntry> entryBuffer;
    for (auto methodId : Reader::Resolve(session, def.GetVirtualMethods())) {
        builder.AddMethod(session, methodId, type.SubTerms());
    }

    // 3.3 add new subtable for current type (even if new methods were not added)
    auto tables = flags.Is(TypeKind::INTERFACE) ? &builder.table.interfaceTables : &builder.table.classTables;

    auto entryCountWithoutCurrentClass = builder.table.EntryCount();

    tables->EmplaceBack(MethodTable::SubTable {
        .declaringType = thisType,
        .start          = entryCountWithInterfaces,
        .end            = builder.table.EntryCount(),
    });

    bool conflictFound = builder.VerifyAndResolveConflicts(session, oldEntryCount, entryCountWithoutCurrentClass);
    if (conflictFound) {
        return std::nullopt;
    }
    return builder.table;
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
        auto gterm = TermManager::Of(session).Globalize(term);
        result     = GetMethodTableCached(session, gterm);
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
    // FIXME: this global caching is not efficient. Either remove caching entirely or use per-session cache.
    std::unordered_map<GlobalTerm, std::shared_ptr<MethodTable>, Term::Hasher> tables;

    /// Returns an method table for the given type definition.
    std::optional<std::shared_ptr<MethodTable>> GetMethodTableCached(Session& session, GlobalTerm type) override
    {
        auto& tables = this->tables;

        auto it = tables.find(type);
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
        tables.insert_or_assign(type, res);
        return res;
    }
};

struct LockedMethodTableManager : public MethodTableManager {
    CachingMethodTableManager delegate;
    std::mutex lock;

    std::optional<std::shared_ptr<MethodTable>> GetMethodTableCached(Session& session, GlobalTerm type) override
    {
        std::lock_guard guard(lock);
        return delegate.GetMethodTableCached(session, type);
    }
};

std::unique_ptr<MethodTableManager> MethodTableManager::NewInstance()
{
    return std::make_unique<LockedMethodTableManager>();
}

static Descripted stream(cerr, "[MT] ");
Logging::Logger Log::mt(&stream, Logging::Level::ERROR);

} // namespace Engine
