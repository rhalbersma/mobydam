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

/* endver.c: verify correctness of endgame database and access functions */

#include "test.h"

extern u64 end_acc[7];         /* access statistics */
extern s32 db_threshold;       /* egdb win/loss score cutoff threshold */

bool debug_info = FALSE;
u64 end_tick;
int end_pos[8];                /* for endgame verification */
int end_pc[8] = {MW, MB, -1, -1, -1, -1, -1, -1};
char db_dirs[PATH_MAX] = ".";  /* directory/ies of database files */

/* check position's egdb value against search value */
/* bb -> the board */
static void comp_ce(bitboard *bb)
{
    movelist list;
    bool b;
    int  i, m;
    s32 v, pvv, score;

    empty_board(bb);
    bb->side = (end_pc[0] == MW || end_pc[0] == KW) ? W : B;
    for (i = 0; i < 8; i++)
    {
        if (end_pc[i] >= 0)
        {
            place_piece(bb, end_pos[i], end_pc[i]);
        }
    }
    if (popcount(bb->white | bb->black) > DTWENDPC)
    {
        gen_moves(bb, &list, NULL, FALSE);  /* generate captures only */
        if (list.count != 0)
        {
            return;                     /* skip when capture position */
        }
        b = endgame_wdl(bb, &v);
    }
    else
    {
        b = endgame_dtw(bb, 0, &v);
    }
    if (!b)
    {
        printf("no direct endgame value\n");
        print_board(bb);
        exit(EXIT_FAILURE);
        return;
    }
    gen_moves(bb, &list, NULL, TRUE);       /* generate all moves */
    pvv = -INFIN;
    for (m = 0; m < list.count; m++)
    {
        if (!endgame_value(&list.move[m], 1, &score))
        {
            printf("no endgame value, v=%d\n", v);
            break;
        }
        score = -score;
        if (score > pvv)
        {
            pvv = score;
        }
    }        
    if (popcount(bb->white | bb->black) > DTWENDPC)
    {
        b = ((v >= INFIN - MAXPLY && pvv >= INFIN - MAXPLY)
             || (abs(v) <= MAXPLY && abs(pvv) <= MAXPLY)
             || (v <= -INFIN + MAXPLY && pvv <= -INFIN + MAXPLY));
    }
    else    /* exact distance to win only available with <= 4 pc */
    {
        b = (v == pvv || (abs(v) <= MAXPLY && abs(pvv) <= MAXPLY));
    }
    if (!b)
    {
        printf("mismatch v=%d pvv=%d\n", v, pvv);
        print_board(bb);
    }
}

/* iterate a piece through all positions */
/* bb -> the board */
/* i = index of the piece to iterate */
static void comp_iterate(bitboard *bb, int i)
{
    int p;

    if (i >= elements(end_pc) - 1)
    {
        return;
    }
    for (end_pos[i] = 1; end_pos[i] <= 50; end_pos[i]++)
    {
        if ((end_pos[i] <=  5 && end_pc[i] == MW)
        ||  (end_pos[i] >= 46 && end_pc[i] == MB))
        {
            continue;
        }
        for (p = 0; p < i; p++)
        {
#pragma GCC diagnostic ignored "-Warray-bounds"
            if (end_pos[i] == end_pos[p]
            || (end_pos[i] < end_pos[p] && end_pc[i] == end_pc[p]))
            {
                break;
            }
        }
        if (p < i)
        {
            continue;
        }
        if (end_pc[i + 1] >= 0)
        {
            comp_iterate(bb, i + 1); /* recurse */
        }
        else
        {
            comp_ce(bb);
        }
    }
}

/* the program entry point */
int main(int argc, char *argv[])
{
    int i, pc, opt;
    bool checkend = FALSE;
    bitboard brd;

    while (TRUE)
    {
        opt = getopt(argc, argv, "cde:");
        if (opt == -1) 
        {
            break;
        }
        switch (opt) 
        {
        case 'c':
            checkend = TRUE;
            break;
        case 'd':
            debug_info = TRUE;
            break;
        case 'e':
            strncpy(db_dirs, optarg, sizeof db_dirs - 1);
            break;
        default:
            printf("Usage: %s [-c] [-d] [-e dbdir] piecelist\n", argv[0]);
            printf("  -c = check endgame db file integrity\n"
                   "  -d = print lots of extra debug info\n"
                   "  -e dbdir = directory holding database files\n"
                   "       (or multiple colon-separated directories)\n"
                   "       (default: current directory)\n"
                   "  piecelist = two to six pieces, e.g. wWbBBB\n"
                   "       (side to move is color of first piece)\n");
            exit(EXIT_FAILURE);
        }
    }

    if (optind < argc)
    {
        for (i = 0; i < 8; i++)
        {
            pc = argv[optind][i];
            if (pc == 'w')
            {
                end_pc[i] = MW;
            }
            else if (pc == 'W')
            {
                end_pc[i] = KW;
            }
            else if (pc == 'b')
            {
                end_pc[i] = MB;
            }
            else if (pc == 'B')
            {
                end_pc[i] = KB;
            }
            else
            {
                break; /* out of for loop */
            }
        }
    }

    init_enddb(db_dirs);
    if (checkend)
    {
        check_enddb();
    }
    empty_board(&brd);
    comp_iterate(&brd, 0);

    printf("egdb err=%" PRIu64 " 2pc=%" PRIu64 " 3pc=%" PRIu64 " 4pc=%" PRIu64
           " 5pc=%" PRIu64 " 6pc=%" PRIu64 "\n", end_acc[0], 
           end_acc[2], end_acc[3], end_acc[4], end_acc[5], end_acc[6]);

    return EXIT_SUCCESS;
}
