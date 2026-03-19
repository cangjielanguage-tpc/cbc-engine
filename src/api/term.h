#pragma once

#include "cbc_type_kind.h"
#include "engine/symlevel/terms.h"

namespace API {

/**
 * @class Term
 */
class Term {
public:
    virtual CbcTypeKind Kind() = 0;

    virtual uint32_t Length() = 0;

    virtual Term* Subterm(uint32_t idx) = 0;

protected:
    virtual ~Term() = default;
};

} // namespace API
