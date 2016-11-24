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

/* perftval.c: checking symmetry of evaluation function */

#include "test.h"

/* checking symmetry of evaluation function */
/* by expanding an initial board position and */
/* evaluating each node and its inverse */

bool debug_info;               /* print extra debug info */
char db_dirs[PATH_MAX] = ".";  /* directory/ies of database files */
u64 eval_count;

/* build a tree in the perft-manner and evaluate the nodes */
/* bb -> current board */
/* depth = remaining levels to generate */
/* returns: nr. of nodes generated */
static u64 perftval(bitboard *bb, int depth)
{
    u64 nodes;
    movelist list;
    int i;
    s32 score, inv_score;

    gen_moves(bb, &list, NULL, depth > 0);

    if (depth == 0 && list.count == 0)
    {
        score = eval_board(bb);
        eval_count++;
        invert_board(bb);
        inv_score = eval_board(bb);
        eval_count++;
        if (score != inv_score)
        {
            debug_info = TRUE;
            invert_board(bb); /* back to original board */
            printf("\n");
            print_board(bb);
            score = eval_board(bb); /* and print individual weights */
            printf("score=%d\n", score);
            invert_board(bb);
            print_board(bb);
            inv_score = eval_board(bb); /* and print individual weights */
            printf("score=%d\n", inv_score);
            printf("error: score=%d inv_score=%d\nhit enter\n", score, inv_score);
            getchar();
            exit(EXIT_FAILURE);
        }
        return 1;
    }

    nodes = 0;
    for (i = 0; i < list.count; i++)
    {
        nodes += perftval(&list.move[i], depth - 1); /* recurse */
    }
    return nodes;
}

/* the program entry point */
int main(int argc, char *argv[])
{
    int opt, d, dmax = 1;
    u64 nodes;
    struct timeval tv1, tv2;
    double interval;
    bitboard brd;

    while (TRUE)
    {
        opt = getopt(argc, argv, "c:de:");
        if (opt == -1) 
        {
            break;
        }
        switch (opt) 
        {
        case 'c':
            dmax = atoi(optarg);
            break;
        case 'd':
            debug_info = TRUE;
            break;
        case 'e':
            strncpy(db_dirs, optarg, sizeof db_dirs - 1);
            break;
        default:
            printf("Usage: %s [-c n] [-d] [-e dbdir] FEN\n", argv[0]);
            printf("  -c n = depth count (default is 1)\n"
                   "  -d = print lots of extra debug info\n"
                   "  -e dbdir = directory holding database files\n"
                   "       (or multiple colon-separated directories)\n"
                   "       (default: current directory)\n"
                   "  FEN = initial board position to evaluate\n");
            exit(EXIT_FAILURE);
        }
    }

    init_board(&brd);
    if (optind < argc)
    {
        if (!setup_fen(&brd, argv[optind]))
        {
            exit(EXIT_FAILURE);
        }
    }
    print_board(&brd);
    init_break(db_dirs);
    eval_count = 0;

    for (d = 1; d <= dmax; d++)
    {
        gettimeofday(&tv1, NULL);
        nodes = perftval(&brd, d);
        gettimeofday(&tv2, NULL);
        interval = tv2.tv_sec - tv1.tv_sec +
            (tv2.tv_usec - tv1.tv_usec)/1000000.0;
        printf("perftval(%d) %" PRIu64 " nodes, %" PRIu64 
               " evals, %.2f sec, %.0f kN/s, %.0f kE/s\n",
               d, nodes, eval_count, interval, 
               nodes/(1000.0*interval), eval_count/(1000.0*interval));
    }
    return EXIT_SUCCESS;
}
