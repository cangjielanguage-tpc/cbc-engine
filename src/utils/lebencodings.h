#pragma once

#include <cstdint>

namespace LEB {

// 10 bytes are enough to fit 64-bit ulebs
constexpr auto MAX_SIZE = 10;

/// Decode unsigned LEB from segment `[*bytes, end)`.
/// The method advances pointer at `*bytes`.
uint64_t DecodeULEB(char** bytes, char const* end);

/// Decode signed LEB from segment `[*bytes, end)`.
/// The method advances pointer at `*bytes`.
int64_t DecodeSLEB(char** bytes, char const* end);

/// Encode signed LEB into segment `[*bytes, end)`.
/// The method advances pointer at `*bytes`.
void EncodeSLEB(int64_t value, char** bytes, char const* end);

/// Encode unsigned LEB into segment `[*bytes, end)`.
/// The method advances pointer at `*bytes`.
void EncodeULEB(uint64_t value, char** bytes, char const* end);

} // namespace LEB
