
// Sets up common environment for Shay Green's libraries.
//
// Don't modify this file directly; #define HAVE_CONFIG_H and put your
// configuration into "config.h".

// Copyright (C) 2004-2005 Shay Green.

#ifndef BLARGG_COMMON_H
#define BLARGG_COMMON_H

// Determine compiler's language support

#if defined (__MWERKS__)
#elif defined (_MSC_VER)
#elif defined (__GNUC__)
#elif defined (__MINGW32__)
#elif __cplusplus < 199711
    #define BLARGG_NEW new
    #define STATIC_CAST( type ) (type)

#endif

// Set up boost
#define BOOST_MINIMAL 1

#define BLARGG_BEGIN_NAMESPACE( name )
#define BLARGG_END_NAMESPACE

#ifndef BOOST_MINIMAL
    #define BOOST boost
#endif

#ifndef BOOST
    #if BLARGG_USE_NAMESPACE
        #define BOOST boost
    #else
        #define BOOST
    #endif
#endif

#undef BLARGG_BEGIN_NAMESPACE
#undef BLARGG_END_NAMESPACE
#if BLARGG_USE_NAMESPACE
    #define BLARGG_BEGIN_NAMESPACE( name ) namespace name {
    #define BLARGG_END_NAMESPACE }
#else
    #define BLARGG_BEGIN_NAMESPACE( name )
    #define BLARGG_END_NAMESPACE
#endif

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

// BLARGG_NEW is used in place of 'new' to create objects. By default,
// nothrow new is used.
#ifndef BLARGG_NEW
    #define BLARGG_NEW new (std::nothrow)
#endif

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
