#include "literals.h"
#include "utils/assertion.h"

namespace Interpretation {

Literal const& LiteralTable::operator[](std::size_t i) const {
    return this->at(i);
}

Literal const& LiteralTable::at(std::size_t i) const {
    ASSERT(i < _byteSize);
    auto tbl = (Literal const*) _table;
    return tbl[i];
}

} // Interpretation
