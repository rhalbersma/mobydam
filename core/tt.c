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

/* tt.c: the transposition table functions */

#include "core.h"

ttentry *trans_tbl;   /* the transposition table */
u32 tt_mask;          /* masking unused addressing bits and slot selection */
u64 hash_init;        /* initializer for hash function */

/* flush transposition table */
/* it gets a new initial value for the hash function, */
/* thus no need to zero out the whole table */
void flush_tt(void)
{
    hash_init  = (u64)rand();
    hash_init += (u64)rand() << 32;
}

/* wipe transposition table */
/* use instead of flush_tt for somewhat reproducible timing tests */
void wipe_tt(void)
{
    u32 i;

    for (i = 0; i < tt_mask + 4; i++)
    {
        trans_tbl[i].ttsig = 0;
    }
    printf("wiped tt with %u entries\n", i);
    hash_init = 0x0ecf2aaef2c937b6ULL;
}

/* initialize transposition table */
/* exp = exponent of transposition table size */
/*       (nr. of entries is 2^exp) */
/* returns: TRUE if successful */
bool init_tt(u32 exp)
{
    u32 ttentries;

    ttentries = (1 << exp);
    /* align on cache line boundary to improve performance */
#ifdef _WIN32
    trans_tbl = _aligned_malloc(ttentries*sizeof(ttentry), 64);
    if (trans_tbl == NULL)
#else
    if (posix_memalign((void **)&trans_tbl, 64, ttentries*sizeof(ttentry)) != 0)
#endif
    {
        return FALSE;
    }
    /* mask's 2 lsb's are zero, to have 4 slots per bucket (1 cache line) */
    tt_mask = ttentries - 4;
    printf("created tt with %u entries (2^%u), size=%uMiB\n",
           ttentries, exp, (1 << (exp - 20))*(u32)sizeof(ttentry));
    /* force os to commit the memory now */
    wipe_tt();
    return TRUE;
}

/* probe transposition table for current board position */
/* bb -> current board */
/* ply = ply level */
/* depth = search depth */
/* alpha = alpha value */
/* beta = beta value */
/* out: scoreptr -> value of position */
/* out: bestptr -> collapsed best move, or NULL */
/* returns: TRUE if found in table */
bool probe_tt(bitboard *bb, int ply, int depth, s32 alpha, s32 beta, s32 *scoreptr, u64 *bestptr)
{
    ttentry *ttslot;
    u32 ttsig;
    s32 score;
    u64 a, b, c;

    /* scramble board position into a hash */
    a = bb->white + hash_init;
    b = bb->black + hash_init;
    c = bb->kings + 0x9e3779b97f4a7c13ULL; /* "golden ratio", arbitrary value */
    mix64(a, b, c);

    ttslot = &trans_tbl[c & tt_mask];
    ttsig = ((u32)b ^ bb->side);

    /* check max 4 slots for our signature */
    /* 4 slots share 1 cache line, so after retrieving the first one */
    /* from main memory, the other 3 are also cached, and fast to access */
    if (ttslot[0].ttsig != ttsig)
    {
        if (ttslot[1].ttsig != ttsig)
        {
            if (ttslot[2].ttsig != ttsig)
            {
                if (ttslot[3].ttsig != ttsig)
                {
                    return FALSE;
                }
                ttslot++;
            }
            ttslot++;
        }
        ttslot++;
    }

    /* give caller the collapsed best move, */
    /* to be used for move ordering if depth is insufficient */
    /* (also used to reconstruct and print the pv) */
    if (bestptr != NULL)
    {
        *bestptr = ttslot->bestmove;
    }

    if (ttslot->depth >= depth)
    {
        score = ttslot->score;
        /* adjust dtw score for root node level */
        if (score > INFIN - MAXEXACT)
        {
            score -= ply;
        }
        else if (score < MAXEXACT - INFIN)
        {
            score += ply;
        }

        if (ttslot->betabound)
        {
            if (score >= beta)
            {
                /* beta-bound result found */
                *scoreptr = score;
                return TRUE;
            }
            if (score > alpha)
            {
                /* not a definite result, but we can improve alpha bound */
                *scoreptr = score;
            }
        }
        else if (ttslot->alphabound)
        {
            if (score <= alpha)
            {
                /* alpha-bound result found */
                *scoreptr = score;
                return TRUE;
            }
        }
        else
        {
            /* exact result found */
            *scoreptr = score;
            return TRUE;
        }
    }
#ifdef PF
    /* if it is likely that the bestmove's board position */
    /* will be probed soon, prefetch its tt entry cache line */
    if (bestptr != NULL && depth > 1)
    {
        u64 bestmove = *bestptr;

        /* first, a little sleight of bit to construct the */
        /* board contents using the collapsed bestmove */
        if (bb->side == W)
        {
            /* remove any captured pieces */
            b = bb->black & bestmove;
            /* update moving piece */
            a = bestmove - b;

            if ((bb->white & bb->kings & ~bestmove) != 0)
            {
                /* the moving piece is a king */
                /* update moving king and remove any captured kings */
                c = (bb->kings ^ bb->white ^ a) & bestmove;
            }
            else
            {
                /* remove any captured kings and do promotion */
                c = (bb->kings & bestmove) | (a & ROW1);
            }
        }
        else
        {
            /* remove any captured pieces */
            a = bb->white & bestmove;
            /* update moving piece */
            b = bestmove - a;

            if ((bb->black & bb->kings & ~bestmove) != 0)
            {
                /* the moving piece is a king */
                /* update moving king and remove any captured kings */
                c = (bb->kings ^ bb->black ^ b) & bestmove;
            }
            else
            {
                /* remove any captured kings and do promotion */
                c = (bb->kings & bestmove) | (b & ROW10);
            }
        }
        /* scramble constructed board position into a hash */
        a += hash_init;
        b += hash_init;
        c += 0x9e3779b97f4a7c13ULL; /* "golden ratio", arbitrary value */
        mix64(a, b, c);

        /* prefetch best move's board position tt entry cache line */
        __builtin_prefetch(&trans_tbl[c & tt_mask], 0); /* for reading */
    }
#endif
    return FALSE;
}

/* store current board position in transposition table */
/* bb -> current board */
/* ply = ply level */
/* depth = search depth */
/* alpha = alpha value */
/* beta = beta value */
/* score = value of position */
/* bestmove -> collapsed best move found by search */
void store_tt(bitboard *bb, int ply, int depth, s32 alpha, s32 beta, s32 score, u64 bestmove)
{
    ttentry *ttslot;
    u32 ttsig;
    u64 oldbest;
    u64 a, b, c;

    /* scramble board position into a hash */
    a = bb->white + hash_init;
    b = bb->black + hash_init;
    c = bb->kings + 0x9e3779b97f4a7c13ULL; /* "golden ratio", arbitrary value */
    mix64(a, b, c);

    ttslot = &trans_tbl[c & tt_mask];
    ttsig = ((u32)b ^ bb->side);

    /* check signature to see if current position is stored in slot 0 */
    if (ttslot[0].ttsig == ttsig)
    {
        oldbest = ttslot[0].bestmove; /* set aside old best move from tt */
    }
    /* if not found in slot 0, try slot 1 */
    else if (ttslot[1].ttsig == ttsig)
    {
        oldbest = ttslot[1].bestmove; /* set aside old best move from tt */
        ttslot[1] = ttslot[0];        /* move slots to make room */
    }
    /* if not found in slot 1, try slot 2 */
    else if (ttslot[2].ttsig == ttsig)
    {
        /* found, set aside old best move from tt */
        oldbest = ttslot[2].bestmove;
        ttslot[2] = ttslot[1];        /* move slots to make room */
        ttslot[1] = ttslot[0];
    }
    /* if not found in slot 2, try slot 3 */
    else if (ttslot[3].ttsig == ttsig)
    {
        /* found, set aside old best move from tt */
        oldbest = ttslot[3].bestmove;
        ttslot[3] = ttslot[2];        /* move slots to make room */
        ttslot[2] = ttslot[1];
        ttslot[1] = ttslot[0];
    }
    else
    {
        oldbest = bestmove;           /* there is no old best move */
        ttslot[3] = ttslot[2];        /* move slots to make room */
        ttslot[2] = ttslot[1];
        ttslot[1] = ttslot[0];
    }
    /* store new data in slot 0 */
    ttslot[0].ttsig = ttsig;
    ttslot[0].depth = depth;
    ttslot[0].score = score;
    ttslot[0].alphabound = (score <= alpha);
    ttslot[0].betabound = (score >= beta);
    /* if alpha bound, prefer old slot's best move, since */
    /* the alpha fail-low's best move is near worthless */
    ttslot[0].bestmove = (score <= alpha) ? oldbest : bestmove;
    /* adjust dtw score for root node level */
    if (score > INFIN - MAXEXACT)
    {
        ttslot[0].score += ply;
    }
    else if (score < MAXEXACT - INFIN)
    {
        ttslot[0].score -= ply;
    }
}

/* recursive part of finding the PV continuation moves in */
/* the transposition table; completeness is not guaranteed, */
/* because table entries may have been overwritten. */
/* bb -> current board */
/* ply = ply level */
static void print_pvmoves(bitboard *bb, int ply)
{
    movelist list;
    u64 bestmove;
    s32 score;
    int m;

    /* find next move, alpha- or beta-bound score is fine, too */
    /* (20 is an arbitrary limit, also preventing cycles */
    if (ply < 20 && probe_tt(bb, 0, 0, INFIN, -INFIN, &score, &bestmove))
    {
        /* generate all valid moves */
        gen_moves(bb, &list, NULL, TRUE);
        for (m = 0; m < list.count; m++)
        {
            if ((list.move[m].white | list.move[m].black) == bestmove)
            {
                print_move(&list.move[m]);
                print_pvmoves(&list.move[m], ply + 1); /* recurse */
                return;
            }
        }
    }
    /* no bestmove found in transposition table or among valid moves */
    printf("\n");    /* terminate the move list */
    print_board(bb); /* also print the resulting board diagram */
}

/* print the principal variation */
/* the ply0 move has been made, find the rest in the tt */
/* ply0mvptr -> the root move to start from */
void print_pv(bitboard *ply0mvptr)
{
    print_move(ply0mvptr);
    print_pvmoves(ply0mvptr, 1);
}
