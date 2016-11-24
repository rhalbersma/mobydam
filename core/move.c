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

/* move.c: 10x10 draughts move generator with bitboards */

#include "core.h"

/* masks to find nearest piece in 'north' diagonals for possible king capture */
/* (relative to bitpos 63) */

#define RAYMASK_NW ((1ULL << 57) | (1ULL << 51) | (1ULL << 45) | (1ULL << 39) \
                  | (1ULL << 33) | (1ULL << 27) | (1ULL << 21))

#define RAYMASK_NE ((1ULL << 58) | (1ULL << 53) | (1ULL << 48) | (1ULL << 43) \
                  | (1ULL << 38) | (1ULL << 33) | (1ULL << 28) | (1ULL << 23))

/* masks to find nearest piece in 'south' diagonals for possible king capture */
/* (relative to bitpos 0) */

#define RAYMASK_SW ((1ULL <<  5) | (1ULL << 10) | (1ULL << 15) | (1ULL << 20) \
                  | (1ULL << 25) | (1ULL << 30) | (1ULL << 35) | (1ULL << 40))

#define RAYMASK_SE ((1ULL <<  6) | (1ULL << 12) | (1ULL << 18) | (1ULL << 24) \
                  | (1ULL << 30) | (1ULL << 36) | (1ULL << 42))

/* statistics */
u64 moves_gencalls;     /* nr. of move generator calls made */
u64 moves_generated;    /* nr. of moves generated */

/* add the capture move to the movelist */
/* listptr -> move list structure being constructed */
/* pcbit = final bit position of capturing piece */
/* captbits = positions of captured pieces */
/* type = capturing piece type, M or K */
__inline__
static void addlist_capt(movelist *listptr, u64 pcbit, u64 captbits, int type)
{
    bitboard move;
    bitboard *bb;
    int i, npcapt;

    npcapt = popcount(captbits); /* number of pieces captured */

    /* discard if longer captures exist */
    if (npcapt < listptr->npcapt)
    {
        return;
    }

    /* discard shorter captures */
    if (npcapt > listptr->npcapt)
    {
        listptr->count = 0;
        listptr->npcapt = npcapt;
    }

    bb = listptr->bb; /* the parent bitboard */

    /* construct the move's resulting bitboard */
    if (bb->side == W)
    {
        move.side = B;
        move.white = bb->white - listptr->frombit + pcbit;
        move.black = bb->black - captbits;
        if (type == M)
        {
            /* remove any captured kings and do promotion */
            move.kings = (bb->kings & ~captbits) | (pcbit & ROW1);
        }
    }
    else
    {
        move.side = W;
        move.white = bb->white - captbits;
        move.black = bb->black - listptr->frombit + pcbit;
        if (type == M)
        {
            /* remove any captured kings and do promotion */
            move.kings = (bb->kings & ~captbits) | (pcbit & ROW10);
        }
    }
    if (type == K)
    {
        /* update moving king and remove any captured kings */
        move.kings = (bb->kings & ~captbits) - listptr->frombit + pcbit;
    }

    if (npcapt >= 4)
    {
        /* check for duplicate (only the order of captures is different) */
        for (i = 0; i < listptr->count; i++)
        {
            if (listptr->move[i].white == move.white &&
                listptr->move[i].black == move.black)
            {
                return;
            }
        }
    }

    /* link to parent board */
    move.parent = bb;

    /* draw info: capture = non-zero, also for print_move when from=to */
    move.moveinfo = conv_to_square(pcbit);

    /* add the move to the list */
    listptr->move[listptr->count] = move;

    /* provide long notation if requested */
    if (listptr->lnptr != NULL)
    {
        /* construct long notation from saved turning points */
        listptr->lnptr[listptr->count].square[0] = 
            conv_to_square(listptr->frombit);
        for (i = 1; i <= npcapt; i++)
        {
            listptr->lnptr[listptr->count].square[i] = 
                conv_to_square(listptr->tp[i]);
        }
        listptr->lnptr[listptr->count].square[i] = 0; /* terminator */
    }
    listptr->count++;
}

/* recursive part of man capture move generation */
/* listptr -> move list structure being constructed */
/* pcbit = current bit position of capturing man */
/* captbits = positions of pieces captured so far */
static void mancapt_part(movelist *listptr, u64 pcbit, u64 captbits)
{
    u64 oppbits;

    /* get opponent pieces that are left on the board */
    oppbits = listptr->oppbits - captbits;

    /* save turning point */
    listptr->tp[popcount(captbits)] = pcbit;

    /* adjacent opponent piece followed by an empty square in nw direction? */
    if (((pcbit >> 12) & (oppbits >> 6) & listptr->empty) != 0)
    {
        mancapt_part(listptr, pcbit >> 12, captbits | (pcbit >> 6));
    }

    /* adjacent opponent piece followed by an empty square in ne direction? */
    if (((pcbit >> 10) & (oppbits >> 5) & listptr->empty) != 0)
    {
        mancapt_part(listptr, pcbit >> 10, captbits | (pcbit >> 5));
    }

    /* adjacent opponent piece followed by an empty square in sw direction? */
    if (((pcbit << 10) & (oppbits << 5) & listptr->empty) != 0)
    {
        mancapt_part(listptr, pcbit << 10, captbits | (pcbit << 5));
    }

    /* adjacent opponent piece followed by an empty square in se direction? */
    if (((pcbit << 12) & (oppbits << 6) & listptr->empty) != 0)
    {
        mancapt_part(listptr, pcbit << 12, captbits | (pcbit << 6));
    }

    /* store the capture move sequence (if no longer sequence found) */
    addlist_capt(listptr, pcbit, captbits, M);
}

/* recursive parts of king capture move generation */
/* listptr -> move list structure being constructed */
/* pcbit = current bit position of capturing king */
/* captbits = positions of pieces captured so far */

/* forward declarations to make compiler happy */
static void kingcapt_ne(movelist *listptr, u64 pcbit, u64 captbits);
static void kingcapt_sw(movelist *listptr, u64 pcbit, u64 captbits);
static void kingcapt_se(movelist *listptr, u64 pcbit, u64 captbits);

/* direction is northwest (-6) */
static void kingcapt_nw(movelist *listptr, u64 pcbit, u64 captbits)
{
    u64 oppbits, ray, nearest;
    u64 origpcbit = pcbit;

    /* get opponent pieces that are left on the board */
    oppbits = listptr->oppbits - captbits;

    /* repeat for every trailing empty square */
    do {
        /* save turning point */
        listptr->tp[popcount(captbits)] = pcbit;

        /* find non-empty squares in sideways direction (ne) */
        ray = (RAYMASK_NE >> __builtin_clzll(pcbit)) & ~listptr->empty;
        /* get MS1B */
        nearest = 1ULL << (63 ^ __builtin_clzll(ray | 1ULL));
        /* is it an opponent piece followed by an empty square? */
        if ((nearest & oppbits & (listptr->empty << 5)) != 0)
        {
            kingcapt_ne(listptr, nearest >> 5, captbits | nearest);
        }

        /* find non-empty squares in sideways direction (sw) */
        ray = (RAYMASK_SW * pcbit) & ~listptr->empty;
        /* get LS1B */
        nearest = ray & -ray;
        /* is it an opponent piece followed by an empty square? */
        if ((nearest & oppbits & (listptr->empty >> 5)) != 0)
        {
            kingcapt_sw(listptr, nearest << 5, captbits | nearest);
        }

        /* store the capture move sequence (if no longer sequence found) */
        addlist_capt(listptr, pcbit, captbits, K);
        pcbit >>= 6;
    } while ((pcbit & listptr->empty) != 0);

    /* found non-empty square, see if we can capture again in same direction */
    if ((pcbit & oppbits & (listptr->empty << 6)) != 0)
    {
        /* save turning point */
        listptr->tp[popcount(captbits)] = origpcbit;

        kingcapt_nw(listptr, pcbit >> 6, captbits | pcbit);
    }
}

/* direction is northeast (-5) */
static void kingcapt_ne(movelist *listptr, u64 pcbit, u64 captbits)
{
    u64 oppbits, ray, nearest;
    u64 origpcbit = pcbit;

    /* get opponent pieces that are left on the board */
    oppbits = listptr->oppbits - captbits;

    /* repeat for every trailing empty square */
    do {
        /* save turning point */
        listptr->tp[popcount(captbits)] = pcbit;

        /* find non-empty squares in sideways direction (nw) */
        ray = (RAYMASK_NW >> __builtin_clzll(pcbit)) & ~listptr->empty;
        /* get MS1B */
        nearest = 1ULL << (63 ^ __builtin_clzll(ray | 1ULL));
        /* is it an opponent piece followed by an empty square? */
        if ((nearest & oppbits & (listptr->empty << 6)) != 0)
        {
            kingcapt_nw(listptr, nearest >> 6, captbits | nearest);
        }

        /* find non-empty squares in sideways direction (se) */
        ray = (RAYMASK_SE * pcbit) & ~listptr->empty;
        /* get LS1B */
        nearest = ray & -ray;
        /* is it an opponent piece followed by an empty square? */
        if ((nearest & oppbits & (listptr->empty >> 6)) != 0)
        {
            kingcapt_se(listptr, nearest << 6, captbits | nearest);
        }

        /* store the capture move sequence (if no longer sequence found) */
        addlist_capt(listptr, pcbit, captbits, K);
        pcbit >>= 5;
    } while ((pcbit & listptr->empty) != 0);

    /* found non-empty square, see if we can capture again in same direction */
    if ((pcbit & oppbits & (listptr->empty << 5)) != 0)
    {
        /* save turning point */
        listptr->tp[popcount(captbits)] = origpcbit;

        kingcapt_ne(listptr, pcbit >> 5, captbits | pcbit);
    }
}

/* direction is southwest (+5) */
static void kingcapt_sw(movelist *listptr, u64 pcbit, u64 captbits)
{
    u64 oppbits, ray, nearest;
    u64 origpcbit = pcbit;

    /* get opponent pieces that are left on the board */
    oppbits = listptr->oppbits - captbits;

    /* repeat for every trailing empty square */
    do {
        /* save turning point */
        listptr->tp[popcount(captbits)] = pcbit;

        /* find non-empty squares in sideways direction (nw) */
        ray = (RAYMASK_NW >> __builtin_clzll(pcbit)) & ~listptr->empty;
        /* get MS1B */
        nearest = 1ULL << (63 ^ __builtin_clzll(ray | 1ULL));
        /* is it an opponent piece followed by an empty square? */
        if ((nearest & oppbits & (listptr->empty << 6)) != 0)
        {
            kingcapt_nw(listptr, nearest >> 6, captbits | nearest);
        }

        /* find non-empty squares in sideways direction (se) */
        ray = (RAYMASK_SE * pcbit) & ~listptr->empty;
        /* get LS1B */
        nearest = ray & -ray;
        /* is it an opponent piece followed by an empty square? */
        if ((nearest & oppbits & (listptr->empty >> 6)) != 0)
        {
            kingcapt_se(listptr, nearest << 6, captbits | nearest);
        }

        /* store the capture move sequence (if no longer sequence found) */
        addlist_capt(listptr, pcbit, captbits, K);
        pcbit <<= 5;
    } while ((pcbit & listptr->empty) != 0);

    /* found non-empty square, see if we can capture again in same direction */
    if ((pcbit & oppbits & (listptr->empty >> 5)) != 0)
    {
        /* save turning point */
        listptr->tp[popcount(captbits)] = origpcbit;

        kingcapt_sw(listptr, pcbit << 5, captbits | pcbit);
    }
}

/* direction is southeast (+6) */
static void kingcapt_se(movelist *listptr, u64 pcbit, u64 captbits)
{
    u64 oppbits, ray, nearest;
    u64 origpcbit = pcbit;

    /* get opponent pieces that are left on the board */
    oppbits = listptr->oppbits - captbits;

    /* repeat for every trailing empty square */
    do {
        /* save turning point */
        listptr->tp[popcount(captbits)] = pcbit;

        /* find non-empty squares in sideways direction (ne) */
        ray = (RAYMASK_NE >> __builtin_clzll(pcbit)) & ~listptr->empty;
        /* get MS1B */
        nearest = 1ULL << (63 ^ __builtin_clzll(ray | 1ULL));
        /* is it an opponent piece followed by an empty square? */
        if ((nearest & oppbits & (listptr->empty << 5)) != 0)
        {
            kingcapt_ne(listptr, nearest >> 5, captbits | nearest);
        }

        /* find non-empty squares in sideways direction (sw) */
        ray = (RAYMASK_SW * pcbit) & ~listptr->empty;
        /* get LS1B */
        nearest = ray & -ray;
        /* is it an opponent piece followed by an empty square? */
        if ((nearest & oppbits & (listptr->empty >> 5)) != 0)
        {
            kingcapt_sw(listptr, nearest << 5, captbits | nearest);
        }

        /* store the capture move sequence (if no longer sequence found) */
        addlist_capt(listptr, pcbit, captbits, K);
        pcbit <<= 6;
    } while ((pcbit & listptr->empty) != 0);

    /* found non-empty square, see if we can capture again in same direction */
    if ((pcbit & oppbits & (listptr->empty >> 6)) != 0)
    {
        /* save turning point */
        listptr->tp[popcount(captbits)] = origpcbit;

        kingcapt_se(listptr, pcbit << 6, captbits | pcbit);
    }
}

/* king capture move generation */
/* for one king, looks in all 4 directions for a possible capture */
/* listptr -> move list structure being constructed */
static void kingcapt_main(movelist *listptr)
{
    u64 ray, nearest, pcbit;

    /* starting position of capturing king */
    pcbit = listptr->frombit;

    /* find non-empty squares in nw direction */
    ray = (RAYMASK_NW >> __builtin_clzll(pcbit)) & ~listptr->empty;
    /* get MS1B */
    nearest = 1ULL << (63 ^ __builtin_clzll(ray | 1ULL));
    /* is it an opponent piece followed by an empty square? */
    if ((nearest & listptr->oppbits & (listptr->empty << 6)) != 0)
    {
        kingcapt_nw(listptr, nearest >> 6, nearest);
    }

    /* find non-empty squares in ne direction */
    ray = (RAYMASK_NE >> __builtin_clzll(pcbit)) & ~listptr->empty;
    /* get MS1B */
    nearest = 1ULL << (63 ^ __builtin_clzll(ray | 1ULL));
    /* is it an opponent piece followed by an empty square? */
    if ((nearest & listptr->oppbits & (listptr->empty << 5)) != 0)
    {
        kingcapt_ne(listptr, nearest >> 5, nearest);
    }

    /* find non-empty squares in sw direction */
    ray = (RAYMASK_SW * pcbit) & ~listptr->empty;
    /* get LS1B */
    nearest = ray & -ray;
    /* is it an opponent piece followed by an empty square? */
    if ((nearest & listptr->oppbits & (listptr->empty >> 5)) != 0)
    {
        kingcapt_sw(listptr, nearest << 5, nearest);
    }

    /* find non-empty squares in se direction */
    ray = (RAYMASK_SE * pcbit) & ~listptr->empty;
    /* get LS1B */
    nearest = ray & -ray;
    /* is it an opponent piece followed by an empty square? */
    if ((nearest & listptr->oppbits & (listptr->empty >> 6)) != 0)
    {
        kingcapt_se(listptr, nearest << 6, nearest);
    }
}

/* generate the capture moves */
/* bb -> current board */
/* listptr -> move list structure being constructed */
static void genmoves_capt(bitboard *bb, movelist *listptr)
{
    u64 empty, tobits, to, men, king, kings;

    empty = ALL50 - bb->white - bb->black;

    if (bb->side == W)
    {
        listptr->oppbits = bb->black;
        men = bb->white & ~bb->kings;
        kings = bb->white & bb->kings;
    }
    else
    {
        listptr->oppbits = bb->white;
        men = bb->black & ~bb->kings;
        kings = bb->black & bb->kings;
    }

    /* per direction, find in one fell swoop the set of men that */
    /* have an adjacent opponent piece followed by an empty square; */
    /* then for each man construct its capture move */

    tobits = (men >> 12) & (listptr->oppbits >> 6) & empty; /* nw */
    while (tobits != 0)
    {
        to = tobits & -tobits;
        tobits -= to;
        listptr->frombit = (to << 12);
        listptr->empty = empty | (to << 12);
        mancapt_part(listptr, to, (to << 6));
    }

    tobits = (men >> 10) & (listptr->oppbits >> 5) & empty; /* ne */
    while (tobits != 0)
    {
        to = tobits & -tobits;
        tobits -= to;
        listptr->frombit = (to << 10);
        listptr->empty = empty | (to << 10);
        mancapt_part(listptr, to, (to << 5));
    }

    tobits = (men << 10) & (listptr->oppbits << 5) & empty; /* sw */
    while (tobits != 0)
    {
        to = tobits & -tobits;
        tobits -= to;
        listptr->frombit = (to >> 10);
        listptr->empty = empty | (to >> 10);
        mancapt_part(listptr, to, (to >> 5));
    }

    tobits = (men << 12) & (listptr->oppbits << 6) & empty; /* se */
    while (tobits != 0)
    {
        to = tobits & -tobits;
        tobits -= to;
        listptr->frombit = (to >> 12);
        listptr->empty = empty | (to >> 12);
        mancapt_part(listptr, to, (to >> 6));
    }

    while (kings != 0)
    {
        /* no fell swoop per direction for kings because of */
        /* leading empty squares; check kings one by one */
        /* for all directions */
        king = kings & -kings;
        kings -= king;
        listptr->frombit = king;
        listptr->empty = empty | king;
        kingcapt_main(listptr);
    }
}

/* generate the non-capture moves */
/* bb -> current board */
/* listptr -> move list structure being constructed */
static void genmoves_noncapt(bitboard *bb, movelist *listptr)
{
    bitboard move;
    bitboard *mvptr;
    u64 tobits, to, from, empty, men, kings;
    int m;

    mvptr = listptr->move;
    empty = ALL50 - bb->white - bb->black;

    /* link to parent board */
    move.parent = bb;

    if (bb->side == W)
    {
        men = bb->white & ~bb->kings;
        move.black = bb->black;
        move.side = B;
        move.moveinfo = 1; /* draw info: it's a man move */

        /* for each 'north' direction, find the set of */
        /* white men that have an adjacent empty square */

        tobits = (men >> 6) & empty; /* nw */
        while (tobits != 0)
        {
            to = tobits & -tobits;
            tobits -= to;
            move.white = bb->white - (to << 6) + to;
            move.kings = bb->kings | (to & ROW1); /* promotion */
            *mvptr++ = move;
        }

        tobits = (men >> 5) & empty; /* ne */
        while (tobits != 0)
        {
            to = tobits & -tobits;
            tobits -= to;
            move.white = bb->white - (to << 5) + to;
            move.kings = bb->kings | (to & ROW1); /* promotion */
            *mvptr++ = move;
        }

        kings = bb->white & bb->kings;
        if (kings != 0)
        {
            move.moveinfo = 0; /* draw info: it's a king move */

            /* for each direction, find the set of white kings */
            /* that have an adjacent empty square */

            tobits = (kings >> 6) & empty; /* nw */
            while (tobits != 0)
            {
                to = tobits & -tobits;
                tobits -= to;
                from = (to << 6);
                do
                {
                    move.white = bb->white - from + to;
                    move.kings = bb->kings - from + to;
                    *mvptr++ = move;
                    /* more empty squares beyond? */
                    to = (to >> 6) & empty;
                } while (to != 0);
            }

            tobits = (kings >> 5) & empty; /* ne */
            while (tobits != 0)
            {
                to = tobits & -tobits;
                tobits -= to;
                from = (to << 5);
                do
                {
                    move.white = bb->white - from + to;
                    move.kings = bb->kings - from + to;
                    *mvptr++ = move;
                    /* more empty squares beyond? */
                    to = (to >> 5) & empty;
                } while (to != 0);
            }

            tobits = (kings << 5) & empty; /* sw */
            while (tobits != 0)
            {
                to = tobits & -tobits;
                tobits -= to;
                from = (to >> 5);
                do
                {
                    move.white = bb->white - from + to;
                    move.kings = bb->kings - from + to;
                    *mvptr++ = move;
                    /* more empty squares beyond? */
                    to = (to << 5) & empty;
                } while (to != 0);
            }

            tobits = (kings << 6) & empty; /* se */
            while (tobits != 0)
            {
                to = tobits & -tobits;
                tobits -= to;
                from = (to >> 6);
                do
                {
                    move.white = bb->white - from + to;
                    move.kings = bb->kings - from + to;
                    *mvptr++ = move;
                    /* more empty squares beyond? */
                    to = (to << 6) & empty;
                } while (to != 0);
            }
        }
    }
    else
    {
        men = bb->black & ~bb->kings;
        move.white = bb->white;
        move.side = W;
        move.moveinfo = 1; /* draw info: it's a man move */

        /* for each 'south' direction, find the set of */
        /* black men that have an adjacent empty square */

        tobits = (men << 5) & empty; /* sw */
        while (tobits != 0)
        {
            to = tobits & -tobits;
            tobits -= to;
            move.black = bb->black - (to >> 5) + to;
            move.kings = bb->kings | (to & ROW10); /* promotion */
            *mvptr++ = move;
        }

        tobits = (men << 6) & empty; /* se */
        while (tobits != 0)
        {
            to = tobits & -tobits;
            tobits -= to;
            move.black = bb->black - (to >> 6) + to;
            move.kings = bb->kings | (to & ROW10); /* promotion */
            *mvptr++ = move;
        }

        kings = bb->black & bb->kings;
        if (kings != 0)
        {
            move.moveinfo = 0; /* draw info: it's a king move */

            /* for each direction, find the set of black kings */
            /* that have an adjacent empty square */

            tobits = (kings >> 6) & empty; /* nw */
            while (tobits != 0)
            {
                to = tobits & -tobits;
                tobits -= to;
                from = (to << 6);
                do
                {
                    move.black = bb->black - from + to;
                    move.kings = bb->kings - from + to;
                    *mvptr++ = move;
                    /* more empty squares beyond? */
                    to = (to >> 6) & empty;
                } while (to != 0);
            }

            tobits = (kings >> 5) & empty; /* ne */
            while (tobits != 0)
            {
                to = tobits & -tobits;
                tobits -= to;
                from = (to << 5);
                do
                {
                    move.black = bb->black - from + to;
                    move.kings = bb->kings - from + to;
                    *mvptr++ = move;
                    /* more empty squares beyond? */
                    to = (to >> 5) & empty;
                } while (to != 0);
            }

            tobits = (kings << 5) & empty; /* sw */
            while (tobits != 0)
            {
                to = tobits & -tobits;
                tobits -= to;
                from = (to >> 5);
                do
                {
                    move.black = bb->black - from + to;
                    move.kings = bb->kings - from + to;
                    *mvptr++ = move;
                    /* more empty squares beyond? */
                    to = (to << 5) & empty;
                } while (to != 0);
            }

            tobits = (kings << 6) & empty; /* se */
            while (tobits != 0)
            {
                to = tobits & -tobits;
                tobits -= to;
                from = (to >> 6);
                do
                {
                    move.black = bb->black - from + to;
                    move.kings = bb->kings - from + to;
                    *mvptr++ = move;
                    /* more empty squares beyond? */
                    to = (to << 6) & empty;
                } while (to != 0);
            }
        }
    }

    /* update the count of moves found */
    listptr->count = (int) (mvptr - listptr->move);

    /* provide long notations if requested */
    if (listptr->lnptr != NULL)
    {
        for (m = 0; m < listptr->count; m++)
        {
            if (bb->side == W)
            {
                listptr->lnptr[m].square[0] = 
                    conv_to_square(bb->white & ~listptr->move[m].white);
                listptr->lnptr[m].square[1] = 
                    conv_to_square(listptr->move[m].white & ~bb->white);
            }
            else
            {
                listptr->lnptr[m].square[0] = 
                    conv_to_square(bb->black & ~listptr->move[m].black);
                listptr->lnptr[m].square[1] = 
                    conv_to_square(listptr->move[m].black & ~bb->black);
            }
            listptr->lnptr[m].square[2] = 0; /* terminator */
        }
    }
}

/* generate the moves for the current bitboard position */
/* the moves in the list are represented by their resulting bitboards. */
/* bb -> current board */
/* listptr -> move list structure to be constructed */
/* lnptr -> long notation array to be constructed, or NULL */
/* genall = if TRUE, generate all valid moves including non-captures */
/*          if FALSE, generate captures only */
void gen_moves(bitboard *bb, movelist *listptr, lnlist *lnptr, bool genall)
{
    listptr->count = 0;
    listptr->npcapt = 0;
    listptr->lnptr = (lnentry *) lnptr;
    listptr->bb = bb;
    genmoves_capt(bb, listptr);

    if (genall && listptr->count == 0)
    {
        genmoves_noncapt(bb, listptr);
    }

    moves_gencalls++;
    moves_generated += listptr->count;
}
