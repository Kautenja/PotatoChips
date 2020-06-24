
// Sets up common environment for Shay Green's libraries.
//
// Don't modify this file directly; #define HAVE_CONFIG_H and put your
// configuration into "config.h".

// Copyright (C) 2004-2005 Shay Green.

#ifndef BLARGG_COMMON_H
#define BLARGG_COMMON_H

#include <cstddef>
#include <cassert>
#include <new>

// blargg_err_t (NULL on success, otherwise error string)
typedef const char* blargg_err_t;
const blargg_err_t blargg_success = 0;

// BLARGG_BIG_ENDIAN and BLARGG_LITTLE_ENDIAN
// Only needed if modules are used which must know byte order.
#if !defined (BLARGG_BIG_ENDIAN) && !defined (BLARGG_LITTLE_ENDIAN)
    #if defined (__powerc) || defined (macintosh)
        #define BLARGG_BIG_ENDIAN 1

    #elif defined (_MSC_VER) && defined (_M_IX86)
        #define BLARGG_LITTLE_ENDIAN 1

    #endif
#endif

// BLARGG_NONPORTABLE (allow use of nonportable optimizations/features)
#ifndef BLARGG_NONPORTABLE
    #define BLARGG_NONPORTABLE 0
#endif
#ifdef BLARGG_MOST_PORTABLE
    #error "BLARGG_MOST_PORTABLE has been removed; use BLARGG_NONPORTABLE."
#endif

// BLARGG_CPU_*
#if !defined (BLARGG_CPU_POWERPC) && !defined (BLARGG_CPU_X86)
    #if defined (__powerc)
        #define BLARGG_CPU_POWERPC 1

    #elif defined (_MSC_VER) && defined (_M_IX86)
        #define BLARGG_CPU_X86 1

    #endif
#endif

#endif  // BLARGG_COMMON_H
