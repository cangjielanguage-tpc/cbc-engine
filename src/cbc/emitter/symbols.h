#ifndef CBC_EMITTER_SYMBOLS_H
#define CBC_EMITTER_SYMBOLS_H

#include <cstdint>
#include <functional>
#include <unordered_map>

#include "cbc/emitter/segment.h"
#include "cbc/isa_rt.h"
#include "interpreter/literals.h"
#include "utils/assertion.h"
#include "utils/span.h"

namespace Cbc {
namespace Emitter {

enum class SymbolKind {
    LABEL,
    PLAIN_VALUE,
};

class Symbol {
public:
    SymbolKind const kind;
    uint32_t const id;

private:
    friend class Label;
    friend class Symbols;

    inline Symbol(SymbolKind _kind, uint32_t _id) : kind(_kind), id(_id) {}
};

class Label {
public:
    uint32_t const id;

    inline Label(Symbol sym) : id(sym.id) { ASSERT(sym.kind == SymbolKind::LABEL); }

    inline operator Symbol() const { return Symbol(SymbolKind::LABEL, id); }

private:
    friend class Symbols;

    inline Label(uint32_t _id) : id(_id) {}
};

/// This class is used in two different scenarios:
/// - As part of emitter, to reference labels or data.
/// - As part of literal table building.
class Symbols {
public:
    Symbols() = default;

    Symbol Value(uint64_t value);
    Symbol Value(int64_t value);
    Symbol Value(int32_t value);
    Symbol Value(uint32_t value);

    Symbol Address(uintptr_t ptr);
    Label NewLabel();
    void Bind(Label label, int32_t position);
    int32_t LabelPosition(Label label) const;

private:
    friend class LiteralTableBuilder;

    // Symbols (and their storage) are separated by kinds.
    // Big value symbols are stored separately from plain values,
    // to reduce memory overhead (since they are not that frequent).
    std::vector<int32_t> labelPositions;
    std::vector<uint64_t> plainValues;

    // TODO: remove constraint
    static_assert(sizeof(uintptr_t) == sizeof(int64_t));
};

class LiteralTableBuilder {
public:
    static constexpr size_t MAX_SIZE = RT::LIT_TABLE_SIZE;

    LiteralTableBuilder(Symbols _symbols) : symbols(_symbols) {}

    /// Register given symbol as used and assign an index in literal table.
    /// Different symbol instances could reference similar literals in the table,
    /// using same indicies.
    uint16_t UseSymbol(Symbol symbol);

    Interpretation::LiteralTable* BuildTable(std::pmr::memory_resource& heap);

    std::vector<uint8_t> table;
    Symbols symbols;
};

class Fixup {
public:
    Fixup(Symbol sym) : symbol(sym) {}

    virtual ~Fixup() {}

    // Returns the distance between the the end of instruction and the label position.
    int32_t Distance(Symbols const& symbols, Label label) const;

    virtual int32_t Size() const = 0;
    virtual void Resolve(
        Segment& segment, Symbols& symbols, std::function<uint16_t(Symbol)> const& relocationConverter
    ) const = 0;

protected:
    friend class Emitter;
    Symbol symbol;
    int32_t position;
};

} // namespace Emitter
} // namespace Cbc

#endif // CBC_EMITTER_SYMBOLS_H
