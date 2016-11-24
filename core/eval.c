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

/* eval.c: static evaluation of the board position */

#include "core.h"

/* the feature names */
#define KINGS 0
#define DEVEL 1
#define TEMPO 2
#define CENTR 3
#define CLASS 4
#define GOLDN 5
#define FLOCK 6
#define CLOCK 7
#define LLOCK 8
#define RLOCK 9
#define DISTR 10
#define OUT22 11
#define OUT24 12

#define PHASES  4

typedef struct {
    char weight[PHASES];
} featentry;

featentry feat[] = {    /* the features and weights per game phase */
    /*    phase:    0   1   2   3 */
    /* KINGS */ {{ 14, 14, 14, 14 }},
    /* DEVEL */ {{ 12, 11,  8,  2 }},
    /* TEMPO */ {{  4,  5,  6, 11 }},
    /* CENTR */ {{ 10, 11, 12, 12 }},
    /* CLASS */ {{  9,  9, 10, 10 }},
    /* GOLDN */ {{ 13, 13,  4,  9 }},
    /* FLOCK */ {{ 13, 13, 13, 13 }},
    /* CLOCK */ {{ 17, 17, 17, 17 }},
    /* LLOCK */ {{ 16, 16, 16, 16 }},
    /* RLOCK */ {{ 15, 15, 15, 15 }},
    /* DISTR */ {{ 11, 11, 12,  4 }},
    /* OUT22 */ {{ 13, 10,  3,  1 }},
    /* OUT24 */ {{ 13, 11, 11,  6 }},
};

s32 king_val[PHASES] = /* a king is valued at VAL_MAN plus: */
{ 4*VAL_MAN/3, 7*VAL_MAN/3, 7*VAL_MAN/3, 7*VAL_MAN/3 };

u64 eval_count;                /* nr. of board evaluations */

/* get the game phase */
/* pcnt = piece count */
/* returns: the game phase (0-3) */
/* 0 for  >=32 pieces */
/* 1 for 24-31 pieces */
/* 2 for 16-23 pieces */
/* 3 for  <=15 pieces */
int game_phase(int pcnt)
{
    int phase;

    phase = 4 - pcnt/8;
    phase = min(phase, 3);
    phase = max(phase, 0);
    return phase;
}

/* evaluate current board position */
/* bb -> current board */
/* returns: evaluation score for side to move */
/* please excuse the mixing of bools and ints */
s32 eval_board(bitboard *bb)
{
    u64 wm, bm, wk, bk;
    s32 score, ftval, tempo;
    int phase;

    eval_count++;
    phase = game_phase(popcount(bb->white | bb->black));

    /* material */
    score = VAL_MAN*(popcount(bb->white) - popcount(bb->black));

    /* breakthroughs */
    score += eval_break(bb);

    /* kings: material and strategic lines/squares */
    ftval = 0;
    if (bb->kings != 0)
    {
        wk = bb->white & bb->kings;
        bk = bb->black & bb->kings;
        score += king_val[phase]*(popcount(wk) - popcount(bk));
        if (wk != 0 && bk != 0)
        {
            /* both sides have kings; reduce the score, because a draw is */
            /* more likely now. (this also discourages allowing mutual */
            /* breakthroughs when ahead) */
            score = score/2;
        }
        ftval +=
            1*popcount(wk & (S01 | S05 | S07 | S11 | S12 | S17 | S18 | S22 |
                             S29 | S33 | S34 | S39 | S40 | S44 | S45 | S46 |
                             S50)) +
            2*popcount(wk & (S01 | S04 | S05 | S06 | S10 | S14 | S15 | S19 |
                             S23 | S28 | S32 | S36 | S37 | S41 | S46 | S47 |
                             S50));
        ftval -=
            1*popcount(bk & (R01 | R05 | R07 | R11 | R12 | R17 | R18 | R22 |
                             R29 | R33 | R34 | R39 | R40 | R44 | R45 | R46 |
                             R50)) +
            2*popcount(bk & (R01 | R04 | R05 | R06 | R10 | R14 | R15 | R19 |
                             R23 | R28 | R32 | R36 | R37 | R41 | R46 | R47 |
                             R50));
        score += ftval << feat[KINGS].weight[phase];
    }
    debugf("KINGS %d\n", ftval);

    wm = bb->white & ~bb->kings;
    bm = bb->black & ~bb->kings;

    /* development of the rear */
    ftval = 0;
    ftval +=
        1*popcount(wm & (S36 | S45)) -
        1*popcount(wm & (S44 | S46)) -
        2*popcount(wm & (S41 | S50));
    ftval -=
        1*popcount(bm & (R36 | R45)) -
        1*popcount(bm & (R44 | R46)) -
        2*popcount(bm & (R41 | R50));
    score += ftval << feat[DEVEL].weight[phase];
    debugf("DEVEL %d\n", ftval);

    /* tempo: degree of advancement */
    ftval = 0;
    ftval +=
        1*popcount(wm & (ROW9 | ROW7 | ROW5 | ROW3)) +
        2*popcount(wm & (ROW8 | ROW7 | ROW4 | ROW3)) +
        4*popcount(wm & (ROW6 | ROW5 | ROW4 | ROW3)) +
        8*popcount(wm & (ROW2));
    ftval -=
        1*popcount(bm & (ROB9 | ROB7 | ROB5 | ROB3)) +
        2*popcount(bm & (ROB8 | ROB7 | ROB4 | ROB3)) +
        4*popcount(bm & (ROB6 | ROB5 | ROB4 | ROB3)) +
        8*popcount(bm & (ROB2));
    score += ftval << feat[TEMPO].weight[phase];
    debugf("TEMPO %d\n", ftval);
    tempo = ftval;

    /* occupation of center */
    ftval = 0;
    ftval +=
        1*popcount(wm & (S27 | S28 | S34 | S37 | S38 | S39)) +
        2*popcount(wm & (S28 | S29 | S32 | S33));
    ftval -=
        1*popcount(bm & (R27 | R28 | R34 | R37 | R38 | R39)) +
        2*popcount(bm & (R28 | R29 | R32 | R33));
    score += ftval << feat[CENTR].weight[phase];
    debugf("CENTR %d\n", ftval);

    /* "classical" configuration */
    ftval = 0;
    if ((wm & (S29 | S32)) == S32)
    {
        ftval += 2*((wm & S28) != 0) +
            ((wm & (S27 | S28)) == (S27 | S28)) +
            (((wm | bm) & S28) == 0);
        /* pos. tempo diff (white more advanced) reduces classical value */
        if (tempo > 0)
        {
            ftval -= tempo;
        }
    }
    if ((bm & (R29 | R32)) == R32)
    {
        ftval -= 2*((bm & R28) != 0) +
            ((bm & (R27 | R28)) == (R27 | R28)) +
            (((bm | wm) & R28) == 0);
        /* neg. tempo diff (black more advanced) reduces classical value */
        if (tempo < 0)
        {
            ftval -= tempo;
        }
    }
    score += ftval << feat[CLASS].weight[phase];
    debugf("CLASS %d\n", ftval);

    /* "kroonschijf", golden piece */
    ftval = 0;
    ftval += ((wm & S48) != 0);
    ftval -= ((bm & R48) != 0);
    score += ftval << feat[GOLDN].weight[phase];
    debugf("GOLDN %d\n", ftval);

    /* "hekstelling", fork lock */
    ftval = 0;
    ftval += (((wm & (S26 | S27 | S31 | S36)) | (bm & (S16 | S18)))
                     == (S26 | S27 | S31 | S36 | S16 | S18) &&
                     popcount(bm & (S22 | S23 | S28)) == 1);
    ftval -= (((bm & (R26 | R27 | R31 | R36)) | (wm & (R16 | R18)))
                     == (R26 | R27 | R31 | R36 | R16 | R18) &&
                     popcount(wm & (R22 | R23 | R28)) == 1);
    score += ftval << feat[FLOCK].weight[phase];
    debugf("FLOCK %d\n", ftval);

    /* "kettingstelling", chain lock */
    ftval = 0;
    ftval -= (((wm & (S27 | S28 | S29)) | (bm & (S22 | S23 | S27 | S29)))
                     == (S22 | S23 | S28));
    ftval += (((bm & (R27 | R28 | R29)) | (wm & (R22 | R23 | R27 | R29)))
                     == (R22 | R23 | R28));
    ftval -= (((wm & (S28 | S29 | S30)) | (bm & (S23 | S24 | S28 | S30)))
                     == (S23 | S24 | S29));
    ftval += (((bm & (R28 | R29 | R30)) | (wm & (R23 | R24 | R28 | R30)))
                     == (R23 | R24 | R29));
    score += ftval << feat[CLOCK].weight[phase];
    debugf("CLOCK %d\n", ftval);

    /* "lange vleugel opsluiting", left-wing lock */
    ftval = 0;
    ftval += (((wm & S25) | (bm & S20)) == (S20 | S25) &&
              (wm & (S30 | S35)) != 0);
    ftval -= (((bm & R25) | (wm & R20)) == (R20 | R25) &&
              (bm & (R30 | R35)) != 0);
    score += ftval << feat[LLOCK].weight[phase];
    debugf("LLOCK %d\n", ftval);

    /* "korte vleugel opsluiting", right-wing lock */
    ftval = 0;
    ftval +=
        (((wm & (S06 | S22 | S26 | S28)) | (bm & (S06 | S11 | S17 | S22)))
         == (S11 | S17 | S26 | S28));
    ftval -=
        (((bm & (R06 | R22 | R26 | R28)) | (wm & (R06 | R11 | R17 | R22)))
         == (R11 | R17 | R26 | R28));
    ftval +=
        (((wm & S26) | (bm & (S16 | S21))) == (S16 | S21 | S26) &&
         (wm & (S27 | S32)) != 0);
    ftval -=
        (((bm & R26) | (wm & (R16 | R21))) == (R16 | R21 | R26) &&
         (bm & (R27 | R32)) != 0);
    score += ftval << feat[RLOCK].weight[phase];
    debugf("RLOCK %d\n", ftval);

    /* distribution of pieces over the wings */
    ftval = 0;
    ftval -= abs(popcount(wm & (COL1 | COL2 | COL3)) -
                 popcount(wm & (COL8 | COL9 | COL10)));
    ftval += abs(popcount(bm & (COL1 | COL2 | COL3)) -
                 popcount(bm & (COL8 | COL9 | COL10)));
    score += ftval << feat[DISTR].weight[phase];
    debugf("DISTR %d\n", ftval);

    /* poorly defended outpost 22, "kerkhof" */
    ftval = 0;
    ftval -= (wm & (S22 | S17)) != 0 &&
        ((wm & (S27 | S32)) != (S27 | S32)) &&
        ((wm & (S28 | S36)) != (S28 | S36) ||
         popcount(bm & (S01 | S02 | S03 | S07 | S08 | S12 | S13 | S18 | S26)) >
         popcount(wm & (S31 | S37 | S41 | S42 | S46 | S47 | S48)));
    ftval += (bm & (R22 | R17)) != 0 &&
        ((bm & (R27 | R32)) != (R27 | R32)) &&
        ((bm & (R28 | R36)) != (R28 | R36) ||
         popcount(wm & (R01 | R02 | R03 | R07 | R08 | R12 | R13 | R18 | R26)) >
         popcount(bm & (R31 | R37 | R41 | R42 | R46 | R47 | R48)));
    score += ftval << feat[OUT22].weight[phase];
    debugf("OUT22 %d\n", ftval);

    /* poorly defended outpost 24, right wing attack */
    ftval = 0;
    ftval -= (wm & S24) &&
        (popcount(wm & (S29 | S33 | S34)) <= 1 ||
         popcount(bm & (S03 | S04 | S05 | S09 | S10 | S13 | S14)) >
         popcount(wm & (S23 | S35 | S40 | S44 | S45 | S49 | S50)));
    ftval += (bm & R24) &&
        (popcount(bm & (R29 | R33 | R34)) <= 1 ||
         popcount(wm & (R03 | R04 | R05 | R09 | R10 | R13 | R14)) >
         popcount(bm & (R23 | R35 | R40 | R44 | R45 | R49 | R50)));
    score += ftval << feat[OUT24].weight[phase];
    debugf("OUT24 %d\n", ftval);

    if (bb->side != W)
    {
        score = -score;
    }
    return score;
}
