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

typedef struct bb {
    u64 white;          /* bit positions of white pieces */
    u64 black;          /* bit positions of black pieces */
    u64 kings;          /* bit positions of white and black kings */
    u32 side;           /* side to move, W or B */
    u32 moveinfo;       /* additional move info */
    struct bb *parent;  /* ptr to previous board in the tree */
} bitboard;

typedef struct {
    u8 square[32];      /* the long notation square numbers */
} lnentry;

typedef struct {
    lnentry move[128];  /* the long notation moves */
} lnlist;

typedef struct {
    int count;          /* number of moves generated */
    int npcapt;         /* number of pieces captured in each move */
    u64 frombit;        /* during movegen: starting position of captor */
    u64 empty;          /* during movegen: empty positions, including frombit */
    u64 oppbits;        /* during movegen: opponents at start of capture */
    u64 tp[32];         /* during movegen: turning points of capture */
    bitboard *bb;       /* during movegen: ptr to old board */
    lnentry *lnptr;     /* ptr to long notation array */
    bitboard move[128]; /* the new boards resulting from the moves */
} movelist;

/* statistics */
extern u64 moves_gencalls;     /* nr. of move generator calls made */
extern u64 moves_generated;    /* nr. of moves generated */

extern void gen_moves(bitboard *bb, movelist *listptr, lnlist *lnptr, bool genall);
