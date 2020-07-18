// Included at the beginning of library source files, after all other #include lines
#ifndef BLARGG_SOURCE_H_
#define BLARGG_SOURCE_H_

#include <cassert>

// Like printf() except output goes to debug log file. Might be defined to do
// nothing (not even evaluate its arguments).
// void dprintf( const char* format, ... );
inline void blargg_dprintf_( const char*, ... ) { }
#undef dprintf
#define dprintf (1) ? (void) 0 : blargg_dprintf_

// BLARGG_SOURCE_BEGIN: If defined, #included, allowing redefition of dprintf and check
#ifdef BLARGG_SOURCE_BEGIN
    #include BLARGG_SOURCE_BEGIN
#endif

#endif  // BLARGG_SOURCE_H_
