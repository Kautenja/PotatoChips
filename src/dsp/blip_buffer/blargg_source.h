
// By default, #included at beginning of library source files

// Copyright (C) 2005 Shay Green.

#ifndef BLARGG_SOURCE_H
#define BLARGG_SOURCE_H

// If debugging is enabled, abort program if expr is false. Meant for checking
// internal state and consistency. A failed assertion indicates a bug in the module.
// void assert(bool expr);
#include <cassert>

// If ptr is NULL, return out of memory error string.
#define BLARGG_CHECK_ALLOC(ptr)   do { if (!(ptr)) return "Out of memory"; } while (0)

#endif  // BLARGG_SOURCE_H
