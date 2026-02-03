#include <cstring>
#include <utility>
#include "symbols.h"

#include "utils/assertion.h"

namespace Cbc {
namespace Emitter {

constexpr int32_t INVALID_POSITION = -1;

Symbol Symbols::Address(uintptr_t ptr) {
    return Value(static_cast<uint64_t>(ptr));
}

Symbol Symbols::Value(int32_t val) {
    return Value(static_cast<int64_t>(val));
}

Symbol Symbols::Value(uint32_t val) {
    return Value(static_cast<uint64_t>(val));
}

Symbol Symbols::Value(int64_t val) {
    return Value(static_cast<uint64_t>(val));
}

Symbol Symbols::Value(uint64_t val) {
    auto id = static_cast<uint32_t>(plainValues.size());
    plainValues.push_back(val);
    return Symbol(SymbolKind::PLAIN_VALUE, id);
}

Label Symbols::NewLabel() {
    auto id = (uint32_t) labelPositions.size();
    labelPositions.push_back(INVALID_POSITION);
    return Label(id);
}

void Symbols::Bind(Label label, int32_t position) {
    ASSERTION(labelPositions.at(label.id) == INVALID_POSITION, "Already initialized");
    labelPositions.at(label.id) = position;
}

int32_t Symbols::LabelPosition(Label label) const {
    return labelPositions.at(label.id);
}

int32_t Fixup::Distance(Symbols const& symbols, Label label) const {
    return symbols.LabelPosition(label) - this->position - Size();
}

uint16_t LiteralTableBuilder::UseSymbol(Symbol symbol) {
    ASSERT(symbol.kind != SymbolKind::LABEL);
    // TODO: - implement deduplication
    //       - Add SymbolKind for literals with size > sizeof(uintptr_t)
    switch (symbol.kind) {
        case SymbolKind::PLAIN_VALUE: {
            auto size = table.size();
            auto step = Interpretation::LITERAL_SIZE;
            ASSERT(size % step == 0);
            ASSERT(size < MAX_SIZE * step);

            auto lit = Interpretation::Literal {
                .u64 = symbols.plainValues.at(symbol.id),
            };

            table.insert(table.end(), &lit.raw[0], &lit.raw[sizeof(lit)]);
            return static_cast<uint16_t>(size / step);
        }
        default:
            ASSERT(false);
            return MAX_SIZE;
    }
}

Interpretation::LiteralTable *LiteralTableBuilder::BuildTable(std::pmr::memory_resource &heap) {
    auto size = table.size();
    auto step = Interpretation::LITERAL_SIZE;
    ASSERT(size % step == 0);
    ASSERT(size < MAX_SIZE * step);

    auto litTable = (Interpretation::LiteralTable*) heap.allocate(sizeof(Interpretation::LiteralTable) + size);
    litTable->_byteSize = size;

    std::copy(table.begin(), table.end(), litTable->_table);
    return litTable;
}

} // namespace Emitter
} // namespace Cbc
