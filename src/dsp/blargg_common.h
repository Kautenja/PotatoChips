// Sets up common environment for Shay Green's libraries.
// To change configuration options, modify blargg_config.h, not this file.

#ifndef BLARGG_COMMON_H
#define BLARGG_COMMON_H

// Uncomment to use zlib for transparent decompression of gzipped files
//#define HAVE_ZLIB_H
// Uncomment to support only the listed game music types. See gme_type_list.cpp
// for a list of all types.
//#define GME_TYPE_LIST gme_nsf_type, gme_gbs_type
// Uncomment to enable platform-specific optimizations
//#define BLARGG_NONPORTABLE 1
// Uncomment to use faster, lower quality sound synthesis
//#define BLIP_BUFFER_FAST 1
// Uncomment if automatic byte-order determination doesn't work
//#define BLARGG_BIG_ENDIAN 1
// Uncomment if you get errors in the bool section of blargg_common.h
//#define BLARGG_COMPILER_HAS_BOOL 1

#include <cstddef>
#include <cstdlib>
#include <cassert>
#include <climits>
#include <climits>
#include <cstdint>

#if INT_MAX >= 0x7FFFFFFF
	typedef int blargg_long;
#else
	typedef long blargg_long;
#endif

#if UINT_MAX >= 0xFFFFFFFF
	typedef unsigned blargg_ulong;
#else
	typedef unsigned long blargg_ulong;
#endif

#endif  // BLARGG_COMMON_H
