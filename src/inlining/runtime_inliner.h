#pragma once

#include "interpreter/code.h"

#include <optional>

namespace Memory {
struct Heap;
}

namespace Cbc {

struct InlinePlan;

using InlineResult = std::optional<Interpretation::ExecBytecodeInfo>;

InlineResult Rebuild(const InlinePlan& plan, Memory::Heap& heap);

} // namespace Cbc
