#include <utility>
#include "emitter.h"

namespace Cbc {
namespace Emitter {

constexpr size_t INVALID_POSITION = SIZE_MAX;

size_t SymbolHash::operator()(const Symbol& s) {
    auto k = (size_t) s.kind;
    auto id = (size_t) s.id;
    return (k << 32u) ^ id;
}

Symbol Symbols::Address(uintptr_t ptr) {
    auto it = ptrToSym.find(ptr);
    if (it != ptrToSym.end()) {
        return Symbol {
            .kind = SymbolKind::ADDRESS,
            .id = it->second
        };
    }
    auto id = ptrSymCount++;
    ptrToSym[ptr] = id;
    return Symbol {
        .kind = SymbolKind::ADDRESS,
        .id = id
    };
}

Symbol Symbols::NewLabel() {
    labelToPosition.push_back(INVALID_POSITION);
    return Symbol {
        .kind = SymbolKind::LABEL,
        .id = labelCount++
    };
}

void Symbols::Bind(Symbol label, size_t position) {
    assertion(label.kind == SymbolKind::LABEL, "Expected label");
    assertion(labelToPosition[label.id] != INVALID_POSITION, "Already initialized");
    labelToPosition[label.id] = (int32_t) position;
}

} // namespace Emitter
} // namespace Cbc
