#include "implicit_exceptions.h"

namespace Interpretation {

void ImplicitException::RegisterExceptionThrower() { RTSupport::Execution::RegisterImplicitExceptionsThrower(); }

} // namespace Interpretation
