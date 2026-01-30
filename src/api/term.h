#pragma once

#include "cbc_type_kind.h"
#include "symlevel/term.h"


namespace API {

/**
 * @class Term
 */
class Term {
public:

    Term(Symlevel::Term term): term(term) {}

    CbcTypeKind Kind();

    Term* Subterm(int idx);

    int Length();

private:
    Symlevel::Term term;
};


} // namespace API
