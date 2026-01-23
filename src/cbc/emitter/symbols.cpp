#include <utility>
#include "symbols.h"

#include "utils/assertion.h"

namespace Cbc {
namespace Emitter {

constexpr int32_t INVALID_POSITION = -1;

Symbol Symbols::Address(uintptr_t ptr) {
    // TODO: checked conversions
    auto id = (uint32_t) plainValues.size();
    plainValues.push_back(ptr);
    return Symbol {
        .kind = SymbolKind::PLAIN_VALUE,
        .id = id,
    };
}


Symbol Symbols::Value(int64_t val) {
    // TODO: checked conversions
    auto id = (uint32_t) plainValues.size();
    plainValues.push_back((uintptr_t) val);
    return Symbol {
        .kind = SymbolKind::PLAIN_VALUE,
        .id = id,
    };
}

Symbol Symbols::NewLabel() {
    auto id = (uint32_t) labelPositions.size();
    labelPositions.push_back(INVALID_POSITION);
    return Symbol {
        .kind = SymbolKind::LABEL,
        .id = id
    };
}

void Symbols::Bind(Label label, int32_t position) {
    ASSERTION(label.kind == SymbolKind::LABEL, "Expected label");
    ASSERTION(labelPositions.at(label.id) == INVALID_POSITION, "Already initialized");
    labelPositions.at(label.id) = position;
}

int32_t Symbols::LabelPosition(Label label) const {
    ASSERTION(label.kind == SymbolKind::LABEL, "Expected label");
    return labelPositions.at(label.id);
}

int32_t Fixup::Distance(Symbols const& symbols, Label label) const {
    ASSERT(label.kind == SymbolKind::LABEL);
    return symbols.LabelPosition(label) - this->position - Size();
}

uint16_t LiteralTableBuilder::UseSymbol(Symbol symbol) {
    ASSERT(symbol.kind != SymbolKind::LABEL);
    // TODO: - implement deduplication
    //       - Add SymbolKind for literals with size > sizeof(uintptr_t)
    switch (symbol.kind) {
        case SymbolKind::PLAIN_VALUE: {
            auto size = table.size();
            table.push_back(symbols.plainValues[symbol.id]);
            ASSERT(size < UINT16_MAX);
            return (uint16_t) size;
        }
        default:
            ASSERT(false);
            return UINT16_MAX;
    }
}

LiteralTable LiteralTableBuilder::BuildTable(std::pmr::memory_resource &heap) {
    auto size = table.size();
    ASSERT(size < UINT16_MAX);

    auto table = (uintptr_t*) heap.allocate(size * sizeof(uintptr_t));
    return LiteralTable {
        .size = size,
        .table = table,
    };
}

} // namespace Emitter
} // namespace Cbc
