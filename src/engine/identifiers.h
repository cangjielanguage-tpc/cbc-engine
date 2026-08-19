#pragma once

#include "engine/symlevel/cbc_file.h"

namespace Engine {
/// An opaque handle to symlevel definitions.

template <typename T> using Identifier    = Symlevel::Identifier<T>;
template <typename T> using RefIdentifier = Symlevel::RefIdentifier<T>;

} // namespace Engine
