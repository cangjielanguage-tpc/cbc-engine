#pragma once

#include "cbc_type_kind.h"


namespace API {

/**
 * @class Term
 */
class Term {
public:

    CbcTypeKind Kind()
    {
        // TODO: implement me
        return CbcTypeKind::INVALID;
    }

    Term* Subterm(int idx)
    {
        // TODO: implement me
        return nullptr;
    }

    int Length()
    {
        // TODO: implement me
        return 0;
    }

private:
    // term: Sym::Term;
};


} // namespace API
