#include "implicit_exceptions.h"
#include "runtimesupport/runtime.h"
#include "utils/assertion.h"

namespace Interpretation {

void* g_exceptionThrower;

void ImplicitException::RegisterExceptionThrower()
{
    g_exceptionThrower = RTSupport::Execution::GetImplicitExceptionsThrower();
}

const void ImplicitException::Throw() const
{
    void* func = NOTNULL(g_exceptionThrower);
    void* res  = RTSupport::Execution::ExecuteCangjieCFunc(func, static_cast<uint64_t>(type), 0, 0);
}

} // namespace Interpretation
