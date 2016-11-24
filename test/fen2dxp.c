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

/* fen2dxp.c: convert FEN(s) to DamExchange GAMEREQ format */

#include "test.h"

/* convert board position to dxp GAMEREQ message */
/* bb -> the board */
static void print_gamereq(bitboard *bb)
{
    int i;
    u64 pcbit;

    printf("%d:%d\n", popcount(bb->white), popcount(bb->black));
    printf("R01Testbed                         %c001001B%c", 
           (bb->side == W)?'W':'Z', (bb->side == W)?'W':'Z');
    for (i = 1; i <= 50; i++)
    {
        pcbit = conv_to_bit(i);
        if (bb->white & pcbit)
        {
            printf("%c", (bb->kings & pcbit)?'W':'w');
        }
        else if (bb->black & pcbit)
        {
            printf("%c", (bb->kings & pcbit)?'Z':'z');
        }
        else
        {
            printf("e");
        }
    }
    printf("\n\n");
}

/* convert FEN(s) to dxp */
int main(int argc, char *argv[])
{
    char line[256];
    int n;
    bitboard brd;

    /* convert FEN to GAMEREQ */

    if (argc > 1)
    {
        /* argument is single FEN */
        if (!setup_fen(&brd, argv[1]))
        {
            exit(EXIT_FAILURE);
        }
        print_gamereq(&brd);
        exit(EXIT_SUCCESS);
    }

    /* no arguments, read pdn file from stdin */
    /* and convert every FEN tag */

    n = 0;
    while (fgets(line, sizeof line, stdin) != NULL)
    {
        if (strncmp(line, "[FEN \"", 6) == 0)
        {
            n++;
            printf("%d - ", n);
            setup_fen(&brd, &line[6]);
            print_gamereq(&brd);
        }
    }
    return EXIT_SUCCESS;
}
