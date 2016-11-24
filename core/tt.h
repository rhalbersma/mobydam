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

/* mix64 -- Source: lookup8.c, by Bob Jenkins, January 4 1997, Public Domain. */
#define mix64(a,b,c) \
{ \
  a -= b; a -= c; a ^= (c>>43); \
  b -= c; b -= a; b ^= (a<<9); \
  c -= a; c -= b; c ^= (b>>8); \
  a -= b; a -= c; a ^= (c>>38); \
  b -= c; b -= a; b ^= (a<<23); \
  c -= a; c -= b; c ^= (b>>5); \
  a -= b; a -= c; a ^= (c>>35); \
  b -= c; b -= a; b ^= (a<<49); \
  c -= a; c -= b; c ^= (b>>11); \
  a -= b; a -= c; a ^= (c>>12); \
  b -= c; b -= a; b ^= (a<<18); \
  c -= a; c -= b; c ^= (b>>22); \
}

typedef struct {
    u32 ttsig;
    s32 score;
    u64 depth      :  8;
    u64 alphabound :  1;
    u64 betabound  :  1;
    u64 bestmove   : 54;
} ttentry;

extern void flush_tt(void);
extern void wipe_tt(void);
extern bool init_tt(u32 exp);
extern bool probe_tt(bitboard *bb, int ply, int depth, s32 alpha, s32 beta, s32 *scoreptr, u64 *bestptr);
extern void store_tt(bitboard *bb, int ply, int depth, s32 alpha, s32 beta, s32 score, u64 bestmove);
extern void print_pv(bitboard *ply0mvptr);
