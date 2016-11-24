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

/* search.c: the principal variation search */

#include "main.h"

typedef struct {               /* set of board positions */
    int count;
    bitboard pos[128];
} poslist;

typedef struct {               /* killer store per ply */
    u64 k1;
    u64 k2;
} kilst;

kilst killer_list[MAXPLY + 1]; /* killer store for all plies */
u32 good_hist[51][64];         /* history of good moves */

s32 iter0_score;               /* static root value before iterations start */
int max_ply;                   /* maximum ply to search */
int db_maxpc;                  /* maximum piece count for wdl lookup */
s32 db_threshold;              /* egdb win/loss score cutoff threshold */
int m_explored;                /* index of move being searched at ply 0 */

u32 start_tick;                /* time tick at start of search */
u32 last_tick;                 /* most recent time tick */
u32 think_time;                /* time budget in milliseconds */

/* statistics */
u64 node_count;                /* nr. of nodes visited */
u64 nonleaf_count;             /* nr. of non-leaf nodes visited */
u64 ttprobe_count;             /* nr. of tt probes */
u64 tthit_count;               /* nr. of tt hits */
u64 ttbest_count;              /* nr. of tt bestmoves */
u64 etctst_count;              /* nr. of etc tests */
u64 etchit_count;              /* nr. of etc hits */
u64 etccut_count;              /* nr. of etc cutoffs */

/* clear history array */
void clear_hist(void)
{
    memset(good_hist, 0, sizeof good_hist);
}

/* fade history array */
/* let info from past moves gradually fade away */
static void fade_hist(void)
{
    int i, j;

    for (i = 1; i <= 50; i++)
    {
        for (j = 1; j <= 50; j++)
        {
            good_hist[i][j] >>= 3;
        }
    }
}

/* sort moves best-first */
/* currently using info from transposition table, */
/* killer moves, and history of earlier good moves */
/* listptr -> move list to be sorted */
/* d = tree depth (distance from leaves) */
/* bestmove = the collapsed best move from the tt */
/* kilptr -> the current ply's killer store */
__inline__
static void sort_moves(movelist *listptr, int d, u64 bestmove, kilst *kilptr)
{
    bitboard move;
    u64 thismove;
    int m, mtt = -1;
#ifdef KIL
    int mk1 = -1, mk2 = -1;
#endif

    /* do tt best move and killers */
    if (bestmove != 0 || d > 2)
    {
        /* find the indexes of the interesting moves */
        for (m = 0; m < listptr->count; m++)
        {
            thismove = listptr->move[m].white | listptr->move[m].black;
            if (thismove == bestmove)
            {
                mtt = m;
            }
#ifdef KIL
            else if (thismove == kilptr->k1)
            {
                mk1 = m;
            }
            else if (thismove == kilptr->k2)
            {
                mk2 = m;
            }
#endif
        }
        /* swap the found moves around to the front of the list */
        m = 0;
        if (mtt == 0)
        {
            m = 1; /* tt best move already at intended place */
        }
        if (mtt > 0)
        {
            /* swap tt best move to head of list */
            move = listptr->move[mtt];
            listptr->move[mtt] = listptr->move[0];
            listptr->move[0] = move;
#ifdef KIL
            if (mk1 == 0)
            {
                mk1 = mtt; /* got a new position */
            }
            if (mk2 == 0)
            {
                mk2 = mtt; /* got a new position */
            }
#endif
            m = 1;
        }
#ifdef KIL
        if (mk1 == m)
        {
            m++; /* killer 1 already at intended place */
        }
        if (mk1 > m)
        {
            /* swap killer 1 to next place */
            move = listptr->move[mk1];
            listptr->move[mk1] = listptr->move[m];
            listptr->move[m] = move;
            if (mk2 == m)
            {
                mk2 = mk1; /* got a new position */
            }
            m++;
        }
        if (mk2 == m)
        {
            m++; /* killer 2 already at intended place */
        }
        if (mk2 > m)
        {
            /* swap killer 2 to next place */
            move = listptr->move[mk2];
            listptr->move[mk2] = listptr->move[m];
            listptr->move[m] = move;
            m++;
        }
#endif

        /* sort on good history for remaining moves m..end */
        if (d > 2 && m < listptr->count - 1)
        {
            u32 g, gh[elements(listptr->move)];
            int i, j;

            /* first make the good_hist scores fast to access */
            for (i = m; i < listptr->count; i++)
            {
                gh[i] = good_hist[move_square(&listptr->move[i], FROM)]
                                 [move_square(&listptr->move[i], TO)];
            }
            /* do insertion sort, is fast for small number of moves */
            for (i = m + 1; i < listptr->count; i++)
            {
                move = listptr->move[i];
                g = gh[i];
                j = i;
                while (j > m && g > gh[j - 1])
                {
                    listptr->move[j] = listptr->move[j - 1];
                    gh[j] = gh[j - 1];
                    j--;
                }
                listptr->move[j] = move;
                gh[j] = g;
            }
        }
    }
}

/* do the recursive principal variation search */
/* bb -> current board */
/* ply = ply level */
/* depth = depth of tree to build */
/* alpha = minimum value to consider */
/* beta = maximum value to consider */
/* returns: backed-up score of move tree */
static s32 pv_search(bitboard *bb, int ply, int depth, s32 alpha, s32 beta)
{
    movelist list;
    u32 tick;
    u64 bestmove;
    s32 origalpha, best, merit;
    int d, m, bestm, pcnt;

    debugf("pv_search enter ply=%d depth=%d side=%d\n",
           ply, depth, bb->side);
    node_count++;
    if (node_count%1024 == 0) /* don't peek at the clock too often */
    {
        tick = get_tick();
        if (side_moving == our_side) /* not pondering */
        {
            /* time check */
            if (tick - start_tick >= think_time ||
                (test_time != 0 && tick - start_tick >= test_time))
            {
                debugf("pv_search time limit reached ply=%d depth=%d\n",
                       ply, depth);
                main_event.movenow = TRUE;
                return 0;
            }
        }
        /* event check every 100ms or so */
        if (tick - last_tick >= 100)
        {
            last_tick = tick;
            {
                /* check for engine event */
                poll_event(0);
                if (main_event.any)
                {
                    debugf("pv_search event occurred ply=%d depth=%d\n",
                           ply, depth);
                    return 0;
                }
            }
        }
    }

    if (bb->white == 0 || bb->black == 0)
    {
        /* side to move has no pieces left */
        debugf("pv_search return ply=%d depth=%d side=%d nopcleft\n",
               ply, depth, bb->side);
        return -INFIN + ply;
    }

    /* check for regulation draw */
    if (is_draw(bb, ply, NULL)) /* in search, avoid any repetitions */
    {
        debugf("pv_search return ply=%d depth=%d side=%d draw\n",
               ply, depth, bb->side);
        /* an aimless king move, even if it's a draw by the book, */
        /* should not stumble into a lost db position */
        if (endgame_value(bb, ply, &best) && best > INFIN - MAXPLY)
        {
            return best;
        }
        return 0;
    }

    best = origalpha = alpha;
    bestmove = 0;
    if (depth > 0) /* no tt probing in quiescence search / leaf nodes, */
    {              /* the memory read stalls are too expensive */
        ttprobe_count++;
        if (probe_tt(bb, ply, depth, alpha, beta, &best, &bestmove))
        {
            debugf("probe_tt hit ply=%d depth=%d side=%d score=%d\n",
                   ply, depth, bb->side, best);
            tthit_count++;
            return best;
        }
    }
    if (bestmove != 0) /* probe_tt found move from lower depth entry */
    {
        ttbest_count++;
    }
    alpha = best;      /* alpha may have been improved by probe_tt */

    /* check DTW endgame database */
    pcnt = popcount(bb->white | bb->black);
    if (pcnt <= DTWENDPC && endgame_dtw(bb, ply, &best))
    {
        debugf("pv_search dtw hit ply=%d depth=%d side=%d score=%d\n",
               ply, depth, bb->side, best);
        return best;
    }

    /* when depth <= 0, generate capture moves only for quiescence search */
    gen_moves(bb, &list, NULL, depth > 0);

    if (list.count == 0 && depth > 0)
    {
        /* side to move can't move */
        debugf("pv_search return ply=%d depth=%d side=%d nomove\n",
               ply, depth, bb->side);
        return -INFIN + ply;
    }

    /* check WDL endgame database, except when we must capture */
    if (pcnt > DTWENDPC && pcnt <= MAXENDPC &&
        (list.count == 0 ||                      /* quiescent leaf node */
         (list.npcapt == 0 && pcnt <= db_maxpc)))/* non-capture interior node */
    {
        if (endgame_wdl(bb, &best))
        {
            debugf("pv_search wdl hit ply=%d depth=%d side=%d score=%d\n",
                   ply, depth, bb->side, best);
            if (depth <= 0 ||             /* quiescent leaf node */
                abs(best) > db_threshold) /* win/loss score found */
            {
                debugf("pv_search wdl cutoff\n");
                return best;
            }
        }
    }

    if (list.count == 0 || ply >= max_ply)
    {
        /* quiescence search complete, arrived at leaf depth */
        return eval_board(bb);
    }

#ifdef CUT
    /* bad move pruning; poor man's ProbCut in the style of Scan 2.0 */
    m = VAL_MAN*9/10; /* margin */

    /* not too close to leaf depth, non-pv node, not in opening, no db win */
    if (depth > 2 && alpha + 1 == beta &&
        game_phase(pcnt) != 0 && beta < INFIN - MAXPLY - m)
    {
        best = pv_search(bb, ply, depth/2, beta + m - 1, beta + m);
        if (best >= beta + m)
        {
            return beta; /* fail-hard seems to work best here */
        }
    }
#endif

    d = depth;
    if (list.count > 1)
    {
        d--;

        /* order moves best-first */
        sort_moves(&list, d, bestmove, &killer_list[ply]);

#ifdef ETC
        /* enhanced transposition cutoffs */
        /* not too close to leaf depth, and not in pv nodes */
        if (d > 4 && alpha + 1 == beta)
        {
            etctst_count++;
            for (m = 0; m < list.count; m++)
            {
                /* see if move leads to a position in the tt */
                if (probe_tt(&list.move[m], ply + 1, d, -beta, -alpha, &best, NULL))
                {
                    etchit_count++;
                    best = -best;
                    /* check for beta cutoff */
                    if (best >= beta)
                    {
                        etccut_count++;
                        debugf("pv_search ETC cut ply=%d depth=%d side=%d "
                               "score=%d\n", ply, depth, bb->side, best);
                        return best;
                    }
                }
            }
        }
#endif
    }

    /* build next tree level */
    nonleaf_count++;

    debugf("pv_search first move\n");
    best = -pv_search(&list.move[0], ply + 1, d, -beta, -alpha);
    bestm = 0;

    /* check for engine event received at greater depth */
    if (main_event.any)
    {
        debugf("pv_search abort after first move ply=%d depth=%d\n",
               ply, depth);
        return 0;
    }

    for (m = 1; m < list.count; m++)
    {
        /* beta cutoff */
        if (best >= beta)
        {
            break; /* out of for loop */
        }
        /* raise lower bound */
        if (best > alpha)
        {
            alpha = best;
        }

#ifdef LMR
        /* late move reductions */
        merit = alpha + 1; /* to do the full search if not reduced */
        if (m >= 3 && alpha + 1 == beta && d > 2 && pcnt >= 8)
        {
            /* reduced depth zero width window search */
            merit = -pv_search(&list.move[m], ply + 1, d - 1 - (m >= 6), 
                               -alpha - 1, -alpha);

            /* check for engine event received at greater depth */
            if (main_event.any)
            {
                debugf("pv_search abort after lmr search ply=%d depth=%d\n",
                       ply, depth);
                return 0;
            }
        }
        if (merit > alpha)
#endif
        {
            /* full depth zero width window search */
            merit = -pv_search(&list.move[m], ply + 1, d, -alpha - 1, -alpha);

            /* check for engine event received at greater depth */
            if (main_event.any)
            {
                debugf("pv_search abort after 0-width search ply=%d depth=%d\n",
                       ply, depth);
                return 0;
            }
        }

        if (merit > best)
        {
            /* found a new best move */
            best = merit;
            bestm = m;
            debugf("pv_search merit > best\n");
            if (best > alpha && best < beta) /* in PV (open window) */
            {
                /* new PV, re-search with full window */
                debugf("pv_search re-search\n");
                merit = -pv_search(&list.move[m], ply + 1, d, -beta, -best);

                /* check for engine event received at greater depth */
                if (main_event.any)
                {
                    debugf("pv_search abort after re-search ply=%d depth=%d\n",
                           ply, depth);
                    return 0;
                }

                if (merit > best)
                {
                    best = merit;
                }
            }
        }
    }

    /* collapse the best move (to save space in the tt) */
    bestmove = list.move[bestm].white | list.move[bestm].black;

#ifdef KIL
    if (best >= beta && list.count > 1)
    {
        /* save as a killer */
        if (killer_list[ply].k1 != bestmove)
        {
            killer_list[ply].k2 = killer_list[ply].k1;
            killer_list[ply].k1 = bestmove;
        }
    }
#endif

    if (depth > 1 && best > origalpha)
    {
        /* save in history of good moves */
        good_hist[move_square(&list.move[bestm], FROM)]
                 [move_square(&list.move[bestm], TO)]
            += (depth - 1)*(depth - 1);
    }

    if (depth > 0)
    {
        /* save score and move in transposition table */
        store_tt(bb, ply, depth, origalpha, beta, best, bestmove);
    }

    debugf("pv_search return ply=%d depth=%d side=%d score=%d\n",
           ply, depth, bb->side, best);
    return best;
}

/* set time budget */
/* depending on game phase and worsening or improving score */
/* bb -> current board */
/* m = index number of the move in the move list, use -m for re-search */
/* score = current score */
/* start = score at start of search */
static void set_budget(bitboard *bb, int m, s32 score, s32 start)
{
    m_explored = abs(m); /* save move index for result display */

    if (score < start - VAL_MAN/10)
    {
        /* try to think our way out of trouble */
        think_time = 3*move_time;
        return;
    }

    think_time = move_time;
    if (game_phase(popcount(bb->white | bb->black)) == 0)
    {
        /* opening, conserve time */
        think_time = think_time/2;
    }

    switch (m)
    {
    case 0:        /* try to finish the PV and the runner-up */
    case 1:
        think_time = 2*think_time;
        break;
    case -1:       /* re-search of runner-up */
    case 2:        /* search of second runner-up */
        think_time = 3*think_time/2;
        break;
    default:
        /* no extra time for the rest, including other re-searches */
        break;
    }

    if (score > start + 7*VAL_MAN/5)
    {
        /* things are going well */
        think_time = 2*think_time/3;
    }
}

/* do the root-level principal variation search */
/* moves have already been generated by the caller */
/* depth = depth of tree to build */
/* listptr -> move list; the best move will be put in front */
/* out: scores -> values of individual moves in the move list, same order */
static void pv_search0(int depth, movelist *listptr, s32 *scores)
{
    bitboard move;
    lnentry moveln;
    s32 alpha, beta, best, merit;
    int d, m;

    node_count++;

#ifdef _DEBUG
    if (debug_info)
    {
        printf("pv_search0 ");
        for (m = 0; m < listptr->count; m++)
        {
            print_move(&listptr->move[m]);
        }
        printf("\n");
    }
#endif
    if (listptr->count == 0)
    {
        debugf("pv_search0 can't move\n");
        scores[0] = -INFIN;
        return;
    }
    d = depth;
    if (listptr->count > 1)
    {
        d--;
    }

    /* adjust time budget */
    set_budget(&listptr->move[0], 0, scores[0], iter0_score);

    /* build first tree level */
    nonleaf_count++;
    alpha = -INFIN;
    beta = INFIN;
    best = -pv_search(&listptr->move[0], 1, d, -beta, -alpha);

    /* check for engine event received at greater depth */
    if (main_event.any)
    {
        debugf("pv_search0 abort first move\n");
        return;
    }

    if (verbose_info)
    {
        printf("%d.%d score=%d pv=", depth, 0, best);
        print_pv(&listptr->move[0]);
    }

    scores[0] = best;

    for (m = 1; m < listptr->count; m++)
    {
        fflush(stdout);

        /* raise lower bound */
        if (best > alpha)
        {
            alpha = best;
        }

        /* adjust time budget */
        set_budget(&listptr->move[m], m, best, iter0_score);

        /* zero width window search */
        merit = -pv_search(&listptr->move[m], 1, d, -alpha - 1, -alpha);

        /* check for engine event received at greater depth */
        if (main_event.any)
        {
            debugf("pv_search0 abort 0-width search\n");
            return;
        }

        scores[m] = merit;
        if (merit > best)
        {
            /* found a new best move */
            best = merit;
            if (verbose_info)
            {
                printf("%d.%d merit=%d pv=", depth, m, merit);
                print_pv(&listptr->move[m]);
            }
            /* pull the move to the head of the list, shifting the rest */
            /* so previous best moves stay near the front */
            move = listptr->move[m];
            memmove(&listptr->move[1], &listptr->move[0], m*sizeof listptr->move[0]);
            listptr->move[0] = move;
            if (listptr->lnptr != NULL) /* long notation too, if present */
            {
                moveln = listptr->lnptr[m];
                memmove(&listptr->lnptr[1], &listptr->lnptr[0], m*sizeof listptr->lnptr[0]);
                listptr->lnptr[0] = moveln;
            }
            memmove(&scores[1], &scores[0], m*sizeof scores[0]);
            scores[0] = best;

            /* re-search is only needed if not the last in the list */
            if (m < listptr->count - 1)
            {
                /* adjust time budget */
                set_budget(&listptr->move[0], -m, best, iter0_score);

                if (verbose_info)
                {
                    printf("%d.%d re-search\n", depth, m);
                }
                merit = -pv_search(&listptr->move[0], 1, d, -beta, -best);

                /* check for engine event received at greater depth */
                if (main_event.any)
                {
                    debugf("pv_search0 abort re-search\n");
                    return;
                }

                if (merit > best)
                {
                    best = merit;
                }
                scores[0] = best;
            }
            if (verbose_info)
            {
                printf("%d.%d new best=%d pv=", depth, m, best);
                print_pv(&listptr->move[0]);
            }
        }
        else
        {
            if (verbose_info)
            {
                printf("%d.%d score=%d move=", depth, m, merit);
                print_move(&listptr->move[m]);
                printf("\n");
            }
        }
    }
    if (verbose_info)
    {
        printf("%d.complete, %u ms, score=%d move=",
               depth, get_tick() - start_tick, scores[0]);
        print_move(&listptr->move[0]);
        printf("\n");
    }
}

/* recursive part of check if capture sequences are equivalent */
/* bb -> current board */
/* posptr -> position list */
/* returns: TRUE if OK, FALSE if storage exceeded */
static bool equiv_search(bitboard *bb, poslist *posptr)
{
    movelist list;
    int m;

    /* generate capture moves only */
    gen_moves(bb, &list, NULL, FALSE);

    if (list.count == 0)
    {
        /* arrived at leaf depth, add this board to the list */
        if (posptr->count >= elements(posptr->pos))
        {
            printf("equiv_search too complex\n");
            return FALSE;
        }
        posptr->pos[posptr->count] = *bb;
        posptr->count++;
        return TRUE;
    }
    /* do the next level of captures */
    for (m = 0; m < list.count; m++)
    {
        if (!equiv_search(&list.move[m], posptr)) /* recurse */
        {
            return FALSE;
        }
    }
    return TRUE;
}

/* check if each capture sequence leads to the same set of board positions, */
/* making them equivalent, and sparing us the need (and time) to search */
/* listptr -> move list */
/* returns: TRUE if equivalent */
static bool equiv_captures(movelist *listptr)
{
    poslist posarray[2];
    int m, n, p;

    /* we hande only multiple captures */
    if (listptr->npcapt == 0 || listptr->count <= 1)
    {
        return FALSE;
    }

    p = 0;
    for (m = 0; m < listptr->count; m++)
    {
        posarray[p].count = 0;
        if (!equiv_search(&listptr->move[m], &posarray[p]))
        {
            return FALSE;
        }
        qsort(posarray[p].pos, posarray[p].count, sizeof(bitboard),
              (__compar_fn_t) bb_compare);
        if (p != 0)
        {
            /* compare second/third/etc. set of sorted positions to the first */
            if (posarray[0].count != posarray[1].count)
            {
                return FALSE;
            }
            for (n = 0; n < posarray[0].count; n++)
            {
                if (bb_compare(&posarray[0].pos[n], &posarray[1].pos[n])
                    != EQUAL)
                {
                    return FALSE;
                }
            }
        }
        p = 1;
    }
    return TRUE;
}

/* let engine determine the next move to be made */
/* listptr -> move list; the best move will be put in front */
/* maxdepth = ultimate iterative search depth */
void engine_think(movelist *listptr, int maxdepth)
{
    bitboard *bb;
    s32 scores[elements(listptr->move)];
    s32 best, nextbest;
    int d, m;

    scores[0] = 0;
    start_tick = last_tick = get_tick();
    fade_hist();

    if (get_bookmove(listptr))
    {
        printf("book move selected\n");
    }
    else if (listptr->count == 1)
    {
        printf("only one valid move, no need to search\n");
    }
    else if (equiv_captures(listptr))
    {
        printf("capture moves are all equivalent, no need to search\n");
    }
    else
    {
        /* the caller generated the moves for the root node */
        moves_gencalls = 1;
        moves_generated = listptr->count;

        /* clear statistics counts */
        node_count = nonleaf_count = 0;
        ttprobe_count = tthit_count = ttbest_count = 0;
        etctst_count = etchit_count = etccut_count = 0;
        end_acc[0] = end_acc[2] = end_acc[3] = 0;
        end_acc[4] = end_acc[5] = end_acc[6] = 0;
        eval_count = 0;
        memset(killer_list, 0, sizeof killer_list);

        /* get a first approximation of the score */
        bb = listptr->move[0].parent;
        if (!endgame_value(bb, 0, &iter0_score))
        {
            iter0_score = eval_board(bb);
        }
        scores[0] = iter0_score;

        /* may cutoff at any 5- or 6-pc win/loss position encountered */
        max_ply = MAXPLY;
        db_threshold = INFIN - max_ply;
        db_maxpc = 6;

        /* iterative deepening */
        for (d = 1; d <= maxdepth; d++)
        {
            pv_search0(d, listptr, scores);

            fflush(stdout);

            if (main_event.movenow)
            {
                /* time is up, we have to make a move */
                main_event.movenow = FALSE;
                break; /* out of iteration */
            }
            if (main_event.any)
            {
                /* search was interrupted */
                return;
            }
            if (test_depth != 0 && d >= test_depth)
            {
                /* depth limit reached */
                break; /* out of iteration */
            }

            bb = &listptr->move[0];
            if (popcount(bb->white | bb->black) <= DTWENDPC)
            {
                if (endgame_dtw(&listptr->move[0], 1, &best))
                {
                    printf("best move's position is in dtw database\n");
                    break;         /* no need for another iteration */
                }
            }

            if (abs(scores[0]) > INFIN - MAXEXACT)
            {
                printf("found win or loss score from dtw database\n");
                break; /* no need for another iteration */
            }

            nextbest = -INFIN;
            for (m = 1; m < listptr->count; m++)
            {
                nextbest = max(nextbest, scores[m]);
            }
            if (nextbest < MAXEXACT - INFIN)
            {
                printf("remaining moves score a loss from dtw database\n");
                break; /* no need for another iteration */
            }

            if (abs(scores[0]) > db_threshold)
            {
                /* found a winning/losing move from wdl database, */
                /* search in next iteration for a quicker win / slower loss */
                /* with fewer pieces, by adjusting the cutoff threshold */
                if (abs(scores[0]) < INFIN - MAX5PLY)
                {
                    /* 6-pc win/loss found, now cutoff at any 5-pc win/loss */
                    max_ply = MAX5PLY;
                    db_maxpc = 5;
                }
                else
                {
                    /* 5-pc win/loss found, now keep searching for dtw */
                    max_ply = MAXEXACT;
                    db_maxpc = 4;
                }
                db_threshold = INFIN - max_ply;
                printf("entering iteration %d with threshold=%d maxply=%d\n",
                       d + 1, db_threshold, max_ply);
            }
        }

        printf("reached depth=%d move=%d\n", d, m_explored);
        printf("nodes total=%" PRIu64 " nonleaf=%" PRIu64 " leaf=%" PRIu64 "\n",
               node_count, nonleaf_count, node_count - nonleaf_count);
        printf("moves calls=%" PRIu64 " generated=%" PRIu64 "\n",
               moves_gencalls, moves_generated);
        printf("tt probes=%" PRIu64 " hits=%" PRIu64 " bestmoves=%" PRIu64 "\n",
               ttprobe_count, tthit_count, ttbest_count);
#ifdef ETC
        printf("etc tests=%" PRIu64 " tthits=%" PRIu64 " cuts=%" PRIu64 "\n",
               etctst_count, etchit_count, etccut_count);
#endif
        printf("egdb err=%" PRIu64 " 2pc=%" PRIu64 " 3pc=%" PRIu64 " 4pc=%"
               PRIu64 " 5pc=%" PRIu64 " 6pc=%" PRIu64 "\n", end_acc[0],
               end_acc[2], end_acc[3], end_acc[4], end_acc[5], end_acc[6]);
        printf("evals=%" PRIu64 " score=%d\n", eval_count, scores[0]);
    }
    return;
}

/* let engine ponder while it is the opponent's turn; */
/* this fills the transposition table, speeding up the */
/* next search for our side */
/* bb -> current board */
/* maxdepth = ultimate iterative search depth */
/* returns: TRUE if further pondering still useful */
bool engine_ponder(bitboard *bb, int maxdepth)
{
    movelist list;
    s32 scores[elements(list.move)];
    int d;

    /* generate the moves for the root node */
    gen_moves(bb, &list, NULL, TRUE);
    if (list.count == 0)
    {
        if (verbose_info)
        {
            printf("pondering but no moves\n");
        }
        return FALSE;
    }
    start_tick = last_tick = get_tick();
    fade_hist();
    memset(killer_list, 0, sizeof killer_list);

    /* may cutoff at any 5- or 6-pc win/loss position encountered */
    max_ply = MAXPLY;
    db_threshold = INFIN - max_ply;
    db_maxpc = 6;

    /* iterative deepening */
    for (d = 1; d <= maxdepth; d++)
    {
        pv_search0(d, &list, scores);

        fflush(stdout);

        main_event.movenow = FALSE; /* not applicable for pondering */
        if (main_event.any)
        {
            /* search was interrupted */
            if (verbose_info)
            {
                printf("pondering aborted, event=%d\n", main_event.any);
            }
            return TRUE;
        }
    }
    if (verbose_info)
    {
        printf("pondering reached max depth\n");
    }
    return FALSE;
}
