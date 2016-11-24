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

/* val.c: evaluate a board position */

#include "test.h"

bool debug_info;               /* print extra debug info */
char db_dirs[PATH_MAX] = ".";  /* directory/ies of database files */

/* add piece list for one color to fen string */
/* fen -> the fen string to construct */
/* pieces = bit positions of the color's pieces */
/* kings = bit positions of the kings on the board */
static void add_fen(char *fen, u64 pieces, u64 kings)
{
    u64 pcbit;
    int sq;

    if (pieces == 0)
    {
        return;
    }
    fen += strlen(fen);
    while (pieces != 0)
    {
        pcbit = pieces & -pieces;
        pieces -= pcbit;
        sq = conv_to_square(pcbit);
        if ((pcbit & kings) != 0)
        {
            *fen++ = 'K';
        }
        if (sq >= 10)
        {
            *fen++ = sq/10 + '0';
        }
        *fen++ = sq%10 + '0';
        *fen++ = ',';
    }
    /* remove last comma and terminate string */
    *--fen = '\0';
}

/* read "print_board()" output from stdin and construct bitboard */
/* bb -> board structure to fill */
/* returns: TRUE if successful */
static bool setup_printedboard(bitboard *bb)
{
    int sq, ret;
    bool ok;

    sq = 0;
    ok = TRUE;
    empty_board(bb);
    do
    {
        ret = fgetc(stdin);
        switch (ret)
        {
        case 'w':
            ok = place_piece(bb, ++sq, MW);
            break;
        case 'W':
            ok = place_piece(bb, ++sq, KW);
            break;
        case 'b':
            ok = place_piece(bb, ++sq, MB);
            break;
        case 'B':
            ok = place_piece(bb, ++sq, KB);
            break;
        case '.':
            sq++;
            break;
        case '(':
            if (sq == 50)
            {
                ret = fgetc(stdin);
                switch (ret)
                {
                case 'w':
                    bb->side = W;
                    return TRUE;
                case 'b':
                    bb->side = B;
                    return TRUE;
                default:
                    return FALSE;
                }
            }
            return FALSE;
        default:
            break;
        }
    } while (sq <= 50 && ok && ret != EOF);
    return FALSE;
}

/* set up test board, do eval and show result */
int main(int argc, char *argv[])
{
    char fen[210];
    s32 s, score, inv_score;
    int opt;
    bitboard brd;

    init_board(&brd);

    while (TRUE)
    {
        opt = getopt(argc, argv, "de:");
        if (opt == -1) 
        {
            break;
        }
        switch (opt) 
        {
        case 'd':
            debug_info = TRUE;
            break;
        case 'e':
            strncpy(db_dirs, optarg, sizeof db_dirs - 1);
            break;
        default:
            printf("Usage: %s [-d] [-e dbdir] [FEN]\n", argv[0]);
            printf("  -d = print lots of extra debug info\n"
                   "  -e dbdir = directory holding database files\n"
                   "       (or multiple colon-separated directories)\n"
                   "       (default: current directory)\n"
                   "  FEN = board position to evaluate\n"
                   "       (default: read graphical board from stdin)\n");
            exit(EXIT_FAILURE);
        }
    }

    if (optind < argc)
    {
        if (!setup_fen(&brd, argv[optind]))
        {
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        if (!setup_printedboard(&brd))
        {
            print_board(&brd);
            exit(EXIT_FAILURE);
        }
        strcpy(fen, (brd.side == W) ? "W:W" : "B:W");
        add_fen(fen, brd.white, brd.kings);
        strcat(fen, ":B");
        add_fen(fen, brd.black, brd.kings);
        printf("%s\n", fen);
    }

    init_enddb(db_dirs);
    init_break(db_dirs);

    print_board(&brd);
    score = eval_board(&brd);
    printf("eval score=%d\n", score);
    s = eval_break(&brd);
    if (s != 0)
    {
        printf("inclusive breakthrough score=%d\n", (brd.side == W) ? s : -s);
    }

    invert_board(&brd);
    if (debug_info)
    {
        print_board(&brd);
    }
    inv_score = eval_board(&brd);
    if (score != inv_score)
    {
        printf("error: eval score=%d inv_score=%d\nhit enter\n", score, inv_score);
        getchar();
        exit(EXIT_FAILURE);
    }

    if (endgame_value(&brd, 0, &score))
    {
        printf("endgame_value score=%d\n", score);
        printf("egdb err=%" PRIu64 " 2pc=%" PRIu64 " 3pc=%" PRIu64 " 4pc=%" 
               PRIu64 " 5pc=%" PRIu64 " 6pc=%" PRIu64 "\n", end_acc[0], 
               end_acc[2], end_acc[3], end_acc[4], end_acc[5], end_acc[6]);
    }
    return EXIT_SUCCESS;
}
