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

/* perft.c: move generation performance test */

#include "test.h"

bool debug_info = FALSE;
bool bulk_counting = FALSE;

/* build a tree and count the nodes */
/* bb -> current board */
/* depth = remaining levels to generate */
/* returns: nr. of nodes generated */
u64 perft(bitboard *bb, int depth)
{
    movelist list;
    int i;
    u64 nodes;

    if (depth == 0)
    {
        return 1;
    }

    gen_moves(bb, &list, NULL, TRUE); /* generate all moves */

    if (depth == 1 && bulk_counting)
    {
        return list.count;
    }

    if (debug_info)
    {
        print_board(bb);
    }

    nodes = 0;
    for (i = 0; i < list.count; i++)
    {
        if (debug_info)
        {
            print_move(&list.move[i]);
            printf("\n");
        }
        nodes += perft(&list.move[i], depth - 1); /* recurse */
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
        opt = getopt(argc, argv, "bc:d");
        if (opt == -1) 
        {
            break;
        }
        switch (opt) 
        {
        case 'b':
            bulk_counting = TRUE;
            break;
        case 'c':
            dmax = atoi(optarg);
            break;
        case 'd':
            debug_info = TRUE;
            break;
        default:
            printf("Usage: %s [-b] [-c n] [-d] FEN\n", argv[0]);
            printf("  -b = bulk counting (default is nobulk)\n"
                   "  -c n = depth count (default is 1)\n"
                   "  -d = print lots of extra debug info\n"
                   "  FEN = initial board position\n");
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

    for (d = 1; d <= dmax; d++)
    {
        gettimeofday(&tv1, NULL);
        nodes = perft(&brd, d);
        gettimeofday(&tv2, NULL);
        interval = tv2.tv_sec - tv1.tv_sec +
            (tv2.tv_usec - tv1.tv_usec)/1000000.0;
        printf("perft(%d) %" PRIu64 " nodes, %.2f sec, %.0f kN/s, %s\n",
               d, nodes, interval, nodes/(1000.0*interval),
               (bulk_counting)?"bulk":"nobulk");
    }
    return EXIT_SUCCESS;
}
