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

/* break.c: breakthrough evaluation */

#include "core.h"

#define MBONUS (1 << 17)
#define LBONUS (VAL_MAN/9)
#define HBONUS (VAL_MAN*4/9)
#define XBONUS (VAL_MAN*5/4)

/* evaluate breakthrough to promotion */
/* bb -> current board */
/* returns: evaluation score for white */
s32 eval_break(bitboard *bb)
{
    u64 wm, bm;
    s32 s, score;

    wm = bb->white & ~bb->kings;
    bm = bb->black & ~bb->kings;
    score = 0;

    /* squares 6..10 */
    /* empty promotion square to left or right (6 only to right) */
    s = popcount(wm & ROW2 & ~((bm & ((bm << 1) | S01)) << 5));
    score += s*XBONUS;

    /* squares 41..45 */
    /* empty promotion square to left or right (45 only to right) */
    s = popcount(bm & ROB2 & ~((wm & ((wm >> 1) | R01)) >> 5));
    score -= s*XBONUS;

    if (wm & ROW3) /* squares 11..15 */
    {
        /* row 3 excl. 15, no directly opposed men */
        s = popcount(wm & (S11 | S12 | S13 | S14) & ~(bm << 11));
        /* 15, only held in check by vulnerable 14 */
        s += (((wm & S15) | (bm & (S04 | S05 | S10))) == S15);
        score += s*MBONUS;

        /* free path, no guards, or a bridge */
        if (bb->side == W)
        {
            s  = popcount(wm & ROW3 & 
                 ~((bm << 6) | (bm << 12) | (((wm | bm) << 1) ^ (bm << 11))));
            s += popcount(wm & (S11 | S12 | S13 | S14) &
                 ~((bm << 5) | (bm << 10) | (((wm | bm) >> 1) ^ (bm << 11))));
        }
        else
        {
            s  = popcount(wm & (S11 | S12 | S13 | S14) & 
                          ~((bm << 1) | (bm << 6) | (bm << 11) | (bm << 12)) &
                          ~((bm << 7) & ((bm >> 1) | (bm << 10))));
            s += popcount(wm & (S11 | S12 | S13 | S14) & 
                          ~((bm >> 1) | (bm << 5) | (bm << 11) | (bm << 10)) &
                          ~((bm << 4) & ((bm << 1) | (bm << 12))));
            s += (((wm & S15) | (bm & (S04 | S05 | S14))) == S15);
        }
        score += s*HBONUS;
    }
    if (bm & ROB3) /* squares 36..40 */
    {
        /* row 8 excl. 36, no directly opposed men */
        s = popcount(bm & (R11 | R12 | R13 | R14) & ~(wm >> 11));
        /* 36, only held in check by vulnerable 37 */
        s += (((bm & R15) | (wm & (R04 | R05 | R10))) == R15);
        score -= s*MBONUS;

        /* free path, no guards, or a bridge */
        if (bb->side != W)
        {
            s  = popcount(bm & ROB3 & 
                 ~((wm >> 6) | (wm >> 12) | (((bm | wm) >> 1) ^ (wm >> 11))));
            s += popcount(bm & (R11 | R12 | R13 | R14) & 
                 ~((wm >> 5) | (wm >> 10) | (((bm | wm) << 1) ^ (wm >> 11))));
        }
        else
        {
            s  = popcount(bm & (R11 | R12 | R13 | R14) & 
                          ~((wm >> 1) | (wm >> 6) | (wm >> 11) | (wm >> 12)) &
                          ~((wm >> 7) & ((wm << 1) | (wm >> 10))));
            s += popcount(bm & (R11 | R12 | R13 | R14) & 
                          ~((wm << 1) | (wm >> 5) | (wm >> 11) | (wm >> 10)) &
                          ~((wm >> 4) & ((wm >> 1) | (wm >> 12))));
            s += (((bm & R15) | (wm & (R04 | R05 | R14))) == R15);
        }
        score -= s*HBONUS;
    }

    if (wm & ROW4) /* squares 16..20 */
    {
        /* row 4 excl. 16, no directly opposed men, */
        /* or men who may move into opposition */
        s = popcount(wm & (S17 | S18 | S19 | S20) & 
                     ~((bm << 11) | (bm << 16) | (bm << 17)));
        /* 16, only held in check by vulnerable 17 */
        s += (((wm & S16) | (bm & (S01 | S06 | S07 | S11))) == S16);
        score += s*MBONUS;

        if (bb->side == W)
        {
            s  = (((wm & S16) | (bm & (S01 | S06 | S07 | S11 | S17))) == S16);
            s += popcount(wm & (S17 | S18 | S19) & ~((bm << 1) | 
                           (bm << 6) | (bm << 11) | (bm << 12) | (bm << 17)));
            s += popcount(wm & (S17 | S18 | S19) & ~((bm >> 1) | 
                           (bm << 5) | (bm << 11) | (bm << 10) | (bm << 16)));
            s += (((wm & S20) | (bm & 
                   (S04 | S05 | S09 | S10 | S14 | S15))) == S20);
        }
        else
        {
            s  = (((wm & S16) | (bm & 
                   (S01 | S02 | S06 | S07 | S11 | S12 | S17))) == S16);
            s += (((wm & S17) | (bm & 
                   (S01 | S02 | S06 | S07 | S11 | S16))) == S17);
            s += (((wm & S17) | (bm & 
                   (S01 | S02 | S03 | S07 | S08 | S12 | S13 | S18))) == S17);
            s += (((wm & S18) | (bm & 
                   (S01 | S02 | S03 | S07 | S08 | S11 | S12 | S17))) == S18);
            s += (((wm & S18) | (bm & 
                   (S02 | S03 | S04 | S08 | S09 | S13 | S14 | S19))) == S18);
            s += (((wm & S19) | (bm & 
                   (S02 | S03 | S04 | S08 | S09 | S12 | S13 | S18))) == S19);
            s += (((wm & S19) | (bm & 
                   (S03 | S04 | S05 | S09 | S10 | S14 | S15 | S20))) == S19);
            s += (((wm & S20) | (bm & 
                   (S03 | S04 | S05 | S09 | S10 | S14 | S15))) == S20);
        }
        score += s*LBONUS;
    }
    if (bm & ROB4) /* squares 31..35 */
    {
        /* row 7 excl. 35, no directly opposed men, */
        /* or men who may move into opposition */
        s = popcount(bm & (R17 | R18 | R19 | R20) & 
                     ~((wm >> 11) | (wm >> 16) | (wm >> 17)));
        /* 35, only held in check by vulnerable 34 */
        s += (((bm & R16) | (wm & (R01 | R06 | R07 | R11))) == R16);
        score -= s*MBONUS;

        if (bb->side != W)
        {
            s  = (((bm & R16) | (wm & (R01 | R06 | R07 | R11 | R17))) == R16);
            s += popcount(bm & (R17 | R18 | R19) & ~((wm >> 1) | 
                           (wm >> 6) | (wm >> 11) | (wm >> 12) | (wm >> 17)));
            s += popcount(bm & (R17 | R18 | R19) & ~((wm << 1) | 
                           (wm >> 5) | (wm >> 11) | (wm >> 10) | (wm >> 16)));
            s += (((bm & R20) | (wm & 
                   (R04 | R05 | R09 | R10 | R14 | R15))) == R20);
        }
        else
        {
            s  = (((bm & R16) | (wm & 
                   (R01 | R02 | R06 | R07 | R11 | R12 | R17))) == R16);
            s += (((bm & R17) | (wm & 
                   (R01 | R02 | R06 | R07 | R11 | R16))) == R17);
            s += (((bm & R17) | (wm & 
                   (R01 | R02 | R03 | R07 | R08 | R12 | R13 | R18))) == R17);
            s += (((bm & R18) | (wm & 
                   (R01 | R02 | R03 | R07 | R08 | R11 | R12 | R17))) == R18);
            s += (((bm & R18) | (wm & 
                   (R02 | R03 | R04 | R08 | R09 | R13 | R14 | R19))) == R18);
            s += (((bm & R19) | (wm & 
                   (R02 | R03 | R04 | R08 | R09 | R12 | R13 | R18))) == R19);
            s += (((bm & R19) | (wm & 
                   (R03 | R04 | R05 | R09 | R10 | R14 | R15 | R20))) == R19);
            s += (((bm & R20) | (wm & 
                   (R03 | R04 | R05 | R09 | R10 | R14 | R15))) == R20);
        }
        score -= s*LBONUS;
    }
    return score;
}

/* initialize breakthrough structures */
/* for future loading of tables from files */
void init_break(char *dirs)
{
    return;
}
