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

/* movegen.c: generate valid moves */

#include "test.h"

/* generate and display all valid moves for a board position */
int main(int argc, char *argv[])
{
    movelist list;
    lnlist longnot;
    bitboard brd;
    int i;

    init_board(&brd);

    if (argc > 1)
    {
        /* argument is single FEN */
        if (!setup_fen(&brd, argv[1]))
        {
            exit(EXIT_FAILURE);
        }
    }

    print_board(&brd);

    gen_moves(&brd, &list, &longnot, TRUE);
    printf("nr. of moves = %d\n", list.count);
    for (i = 0; i < list.count; i++)
    {
        print_move(&list.move[i]);
        print_move_long(&list, i);
        printf("\n");
        print_board(&list.move[i]);
    }

    return 0;
}
