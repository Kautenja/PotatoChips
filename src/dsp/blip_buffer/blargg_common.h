
// Sets up common environment for Shay Green's libraries.
//
// Don't modify this file directly; #define HAVE_CONFIG_H and put your
// configuration into "config.h".

// Copyright (C) 2004-2005 Shay Green.

#ifndef BLARGG_COMMON_H
#define BLARGG_COMMON_H

// BOOST_STATIC_ASSERT( expr )
#ifndef BOOST_STATIC_ASSERT_HPP
#define BOOST_STATIC_ASSERT_HPP

#if defined (_MSC_VER) && _MSC_VER <= 1200
    // MSVC6 can't handle the ##line concatenation
    #define BOOST_STATIC_ASSERT( expr ) struct { int n [1 / ((expr) ? 1 : 0)]; }

#else
    #define BOOST_STATIC_ASSERT3( expr, line ) \
                typedef int boost_static_assert_##line [1 / ((expr) ? 1 : 0)]

    #define BOOST_STATIC_ASSERT2( expr, line ) BOOST_STATIC_ASSERT3( expr, line )

    #define BOOST_STATIC_ASSERT( expr ) BOOST_STATIC_ASSERT2( expr, __LINE__ )

#endif

#endif

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
