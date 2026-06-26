#include "implicit_exceptions.h"

#include "runtimesupport/runtime.h"

namespace Interpretation {

void RegisterExceptionThrower() { RTSupport::Execution::RegisterImplicitExceptionsThrower(); }

} // namespace Interpretation
