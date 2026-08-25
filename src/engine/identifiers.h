#pragma once

#include "engine/image/cbc_file.h"

namespace Engine {
/// An opaque handle to symlevel definitions.

template <typename T> using Identifier    = Image::Identifier<T>;
template <typename T> using RefIdentifier = Image::RefIdentifier<T>;

} // namespace Engine
