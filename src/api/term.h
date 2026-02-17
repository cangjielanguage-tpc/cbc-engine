#pragma once

#include "cbc_type_kind.h"
#include "engine/symlevel/term_val.h"


namespace API {

/**
 * @class Term
 */
class Term {
public:

    Term(Symlevel::TermValue term): term(term) {}

    CbcTypeKind Kind();

    Term* Subterm(int idx);

    int Length();

private:
    Symlevel::TermValue term;
};


} // namespace API
