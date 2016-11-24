/*
    Copyright 2015 Harm Jetten

    This file is part of Moby Dam.

    Moby Dam is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Moby Dam is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Moby Dam.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#define __inline__ __inline
#define PATH_MAX MAX_PATH
#include "../win/getopt.h"
#else
#include <sys/time.h>
#include <unistd.h>
#include <getopt.h>
#endif

#include <limits.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>

#ifdef _WIN32
#define IPV6STRICT
#include <winsock2.h>
typedef int (*__compar_fn_t) (const void *, const void *);
#else
#include <netdb.h>
#include <netinet/in.h> 
#include <sys/socket.h> 
#include <sys/mman.h>
typedef int SOCKET;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define sockerror perror
#define closesocket close
#endif

/* the colors */
#define W 0
#define B 1

/* piece type selector */
#define M 0
#define K 1

/* the piece types; egdb indexing needs them in this order */
#define MW 0
#define KW 1
#define MB 2
#define KB 3

/* from- or to-square selector */
#define FROM 0
#define TO   1

/* ghost squares */
#define G1 (1ULL << 10)
#define G2 (1ULL << 21)
#define G3 (1ULL << 32)
#define G4 (1ULL << 43)

/* square number to bitfield */
#define S01 (1ULL <<  0)
#define S02 (1ULL <<  1)
#define S03 (1ULL <<  2)
#define S04 (1ULL <<  3)
#define S05 (1ULL <<  4)
#define S06 (1ULL <<  5)
#define S07 (1ULL <<  6)
#define S08 (1ULL <<  7)
#define S09 (1ULL <<  8)
#define S10 (1ULL <<  9)
#define S11 (1ULL << 11)
#define S12 (1ULL << 12)
#define S13 (1ULL << 13)
#define S14 (1ULL << 14)
#define S15 (1ULL << 15)
#define S16 (1ULL << 16)
#define S17 (1ULL << 17)
#define S18 (1ULL << 18)
#define S19 (1ULL << 19)
#define S20 (1ULL << 20)
#define S21 (1ULL << 22)
#define S22 (1ULL << 23)
#define S23 (1ULL << 24)
#define S24 (1ULL << 25)
#define S25 (1ULL << 26)
#define S26 (1ULL << 27)
#define S27 (1ULL << 28)
#define S28 (1ULL << 29)
#define S29 (1ULL << 30)
#define S30 (1ULL << 31)
#define S31 (1ULL << 33)
#define S32 (1ULL << 34)
#define S33 (1ULL << 35)
#define S34 (1ULL << 36)
#define S35 (1ULL << 37)
#define S36 (1ULL << 38)
#define S37 (1ULL << 39)
#define S38 (1ULL << 40)
#define S39 (1ULL << 41)
#define S40 (1ULL << 42)
#define S41 (1ULL << 44)
#define S42 (1ULL << 45)
#define S43 (1ULL << 46)
#define S44 (1ULL << 47)
#define S45 (1ULL << 48)
#define S46 (1ULL << 49)
#define S47 (1ULL << 50)
#define S48 (1ULL << 51)
#define S49 (1ULL << 52)
#define S50 (1ULL << 53)

/*  reversed square numbers for black's perspective */
#define R50 (1ULL <<  0)
#define R49 (1ULL <<  1)
#define R48 (1ULL <<  2)
#define R47 (1ULL <<  3)
#define R46 (1ULL <<  4)
#define R45 (1ULL <<  5)
#define R44 (1ULL <<  6)
#define R43 (1ULL <<  7)
#define R42 (1ULL <<  8)
#define R41 (1ULL <<  9)
#define R40 (1ULL << 11)
#define R39 (1ULL << 12)
#define R38 (1ULL << 13)
#define R37 (1ULL << 14)
#define R36 (1ULL << 15)
#define R35 (1ULL << 16)
#define R34 (1ULL << 17)
#define R33 (1ULL << 18)
#define R32 (1ULL << 19)
#define R31 (1ULL << 20)
#define R30 (1ULL << 22)
#define R29 (1ULL << 23)
#define R28 (1ULL << 24)
#define R27 (1ULL << 25)
#define R26 (1ULL << 26)
#define R25 (1ULL << 27)
#define R24 (1ULL << 28)
#define R23 (1ULL << 29)
#define R22 (1ULL << 30)
#define R21 (1ULL << 31)
#define R20 (1ULL << 33)
#define R19 (1ULL << 34)
#define R18 (1ULL << 35)
#define R17 (1ULL << 36)
#define R16 (1ULL << 37)
#define R15 (1ULL << 38)
#define R14 (1ULL << 39)
#define R13 (1ULL << 40)
#define R12 (1ULL << 41)
#define R11 (1ULL << 42)
#define R10 (1ULL << 44)
#define R09 (1ULL << 45)
#define R08 (1ULL << 46)
#define R07 (1ULL << 47)
#define R06 (1ULL << 48)
#define R05 (1ULL << 49)
#define R04 (1ULL << 50)
#define R03 (1ULL << 51)
#define R02 (1ULL << 52)
#define R01 (1ULL << 53)

/* rows, as counted from the top of the board */
#define ROW1 (S01 | S02 | S03 | S04 | S05)
#define ROW2 (S06 | S07 | S08 | S09 | S10)
#define ROW3 (S11 | S12 | S13 | S14 | S15)
#define ROW4 (S16 | S17 | S18 | S19 | S20)
#define ROW5 (S21 | S22 | S23 | S24 | S25)
#define ROW6 (S26 | S27 | S28 | S29 | S30)
#define ROW7 (S31 | S32 | S33 | S34 | S35)
#define ROW8 (S36 | S37 | S38 | S39 | S40)
#define ROW9 (S41 | S42 | S43 | S44 | S45)
#define ROW10 (S46 | S47 | S48 | S49 | S50)

/* rows, as counted from the bottom of the board */
#define ROB10 (S01 | S02 | S03 | S04 | S05)
#define ROB9 (S06 | S07 | S08 | S09 | S10)
#define ROB8 (S11 | S12 | S13 | S14 | S15)
#define ROB7 (S16 | S17 | S18 | S19 | S20)
#define ROB6 (S21 | S22 | S23 | S24 | S25)
#define ROB5 (S26 | S27 | S28 | S29 | S30)
#define ROB4 (S31 | S32 | S33 | S34 | S35)
#define ROB3 (S36 | S37 | S38 | S39 | S40)
#define ROB2 (S41 | S42 | S43 | S44 | S45)
#define ROB1 (S46 | S47 | S48 | S49 | S50)

/* columns, as counted from the left of the board */
#define COL1 (S06 | S16 | S26 | S36 | S46)
#define COL2 (S01 | S11 | S21 | S31 | S41)
#define COL3 (S07 | S17 | S27 | S37 | S47)
#define COL4 (S02 | S12 | S22 | S32 | S42)
#define COL5 (S08 | S18 | S28 | S38 | S48)
#define COL6 (S03 | S13 | S23 | S33 | S43)
#define COL7 (S09 | S19 | S29 | S39 | S49)
#define COL8 (S04 | S14 | S24 | S34 | S44)
#define COL9 (S10 | S20 | S30 | S40 | S50)
#define COL10 (S05 | S15 | S25 | S35 | S45)

/* all squares 1..50 */
#define ALL50 (ROW1 | ROW2 | ROW3 | ROW4 | ROW5 | \
               ROW6 | ROW7 | ROW8 | ROW9 | ROW10)

#define INFIN 2000000000  /* immediate win value */

#define MAXEXACT  64      /* max distance to infin for dtw database score  */
#define MAX5PLY  148      /* max distance to infin for 5-pc database score */
#define MAXPLY   256      /* maximum ply depth, and also                   */
                          /* max distance to infin for 6-pc database score */

typedef uint64_t u64;
typedef int32_t  s32;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t   u8;
typedef u32     bool;

#ifdef _MSC_VER
#define popcount (int)__popcnt64 
__inline static u32 __builtin_ctzll(u64 x) {u32 i; _BitScanForward64(&i, x); return i;}
__inline static u32 __builtin_clzll(u64 x) {u32 i; _BitScanReverse64(&i, x); return 63^i;}
#define __builtin_prefetch(x,y) _mm_prefetch((const CHAR *)(x),_MM_HINT_T0)
#define __builtin_bswap64(x) _byteswap_uint64(x)
#pragma warning(disable: 4146) // unary minus operator applied to unsigned type
#pragma warning(disable: 4267) // conversion from 'size_t' to 'int', possible loss of data
#define strcasecmp _stricmp 
#define strncasecmp _strnicmp 
#define usleep(x) Sleep((x)/1000)
#else
/* POPCNT instruction is supported since the Intel "Nehalem" (Core i) */
/* and AMD "Barcelona" (K10) processors. */
/* Using GCC 4 intrinsic; compile also with -march=native. */
/* (or -march=nehalem for GCC 4.9 and up) */ 
#define popcount __builtin_popcountll
#endif

#ifndef FALSE
#define FALSE ((bool)0)
#endif
#ifndef TRUE
#define TRUE ((bool)1)
#endif

#ifndef min
#define min(x,y) (((x)<(y))?(x):(y))
#endif
#ifndef max
#define max(x,y) (((x)>(y))?(x):(y))
#endif

#ifdef _DEBUG
extern bool debug_info; /* whether to print extra debug info */
#define debugf if (debug_info) printf
#else
#define debugf(...)
#endif

#define EQUAL 0 /* for comparisons */
#define elements(x) ((int) (sizeof (x) / sizeof (x)[0]))
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#include "move.h"
#include "book.h"
#include "break.h"
#include "end.h"
#include "eval.h"
#include "tt.h"
#include "util.h"
