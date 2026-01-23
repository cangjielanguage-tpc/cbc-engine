#ifndef CBC_EMITTER_SYMBOLS_H
#define CBC_EMITTER_SYMBOLS_H

#include <cstdint>
#include <unordered_map>
#include <functional>

#include "utils/span.h"
#include "cbc/emitter/segment.h"

namespace Cbc {
namespace Emitter {

enum class SymbolKind {
    LABEL,
    PLAIN_VALUE,
};

struct Symbol {
    SymbolKind kind;
    uint32_t id;
};

// TODO: introduce struct Label { uint32_t id; } + Sym <-> Label conversions
using Label = Symbol;

/// This class is used in two different scenarios:
/// - As part of emitter, to reference labels or data.
/// - As part of literal table building.
class Symbols {
public:
    Symbols() = default;

    Symbol Value(int64_t value);
    Symbol Address(uintptr_t ptr);
    Label NewLabel();
    void Bind(Label label, int32_t position);
    int32_t LabelPosition(Label label) const;

private:
    friend class LiteralTableBuilder;

    std::vector<int32_t> labelPositions;
    std::vector<uintptr_t> plainValues;

    // TODO: remove constraint
    static_assert(sizeof(uintptr_t) == sizeof(int64_t));
};

struct LiteralTable {
    size_t size;
    uintptr_t* table;
};

class LiteralTableBuilder {
public:
    LiteralTableBuilder(Symbols _symbols)
        : symbols(_symbols) {}

    /// Register given symbol as used and assign an index in literal table.
    /// Different symbol instances could reference similar literals in the table,
    /// using same indicies.
    uint16_t UseSymbol(Symbol symbol);

    LiteralTable BuildTable(std::pmr::memory_resource& heap);

    std::vector<uintptr_t> table;
    Symbols symbols;
};

class Fixup {
public:
    Fixup(Symbol sym)
        : symbol(sym) {}

    virtual ~Fixup() {}

    // Returns the distance between the the end of instruction and the label position.
    int32_t Distance(Symbols const& symbols, Label label) const;

    virtual int32_t Size() const = 0;
    virtual void Resolve(Segment &segment, Symbols& symbols, std::function<void(size_t, Symbol)> const& relocationConverter) const = 0;

protected:
    friend class Emitter;
    Symbol symbol;
    int32_t position;
};


} // namespace Emitter
} // namespace Cbc

#endif // CBC_EMITTER_SYMBOLS_H
