// Sets up common environment for Shay Green's libraries.
// To change configuration options, modify blargg_config.h, not this file.

#ifndef BLARGG_COMMON_H_
#define BLARGG_COMMON_H_

// Uncomment to enable platform-specific optimizations
// #define BLARGG_NONPORTABLE 1
// Uncomment if automatic byte-order determination doesn't work
// #define BLARGG_BIG_ENDIAN 1

#include <cstddef>
#include <cstdlib>
#include <cassert>
#include <climits>
#include <cstdint>

typedef int32_t blargg_long;

typedef uint32_t blargg_ulong;

#endif  // BLARGG_COMMON_H_
