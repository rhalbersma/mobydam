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

/* bookdump.c: show contents of opening book */

#include "test.h"

bool debug_info = FALSE;
bool merge_dup = FALSE;

/* sneaky direct access to loaded book */
extern bitboard *book_positions;
extern size_t book_size;

/* compare two moves lexicographically */
/* bb1 -> move 1 */
/* bb2 -> move 2 */
/* returns:   0 (EQUAL) = same from-to */
/*          < 0 = bb1 from-to < bb2 from-to */
/*          > 0 = bb1 from-to > bb2 from-to */
static int bb_fromto(bitboard *bb1, bitboard *bb2)
{
    int sq1, sq2;

    sq1 = move_square(bb1, FROM);
    sq2 = move_square(bb2, FROM);
    if (sq1 < sq2)
    {
        return -1;
    }
    if (sq1 > sq2)
    {
        return 1;
    }
    sq1 = move_square(bb1, TO);
    sq2 = move_square(bb2, TO);
    if (sq1 < sq2)
    {
        return -1;
    }
    if (sq1 > sq2)
    {
        return 1;
    }
    return EQUAL;
}

/* dump the book (sub)tree hanging off the given board position */
/* bb -> board position to start from */
/* ply = distance from initial position */
static void dump_tree(bitboard *bb, int ply)
{
    char mvstr[7];
    char nagstr[11];
    char *annptr;
    bitboard *bptr;
    movelist list;
    int m, len;
    bool contin;

    gen_moves(bb, &list, NULL, TRUE); /* generate all moves */

    /* sort the moves in from-to numerical order */
    qsort(&list.move[0], list.count, sizeof(bitboard),
          (__compar_fn_t) bb_fromto);

    contin = FALSE;
    for (m = 0; m < list.count; m++)
    {
        /* is the move's board position in the book? */
        bptr = (bitboard *) bsearch(&list.move[m], book_positions,
                                    book_size, sizeof(bitboard),
                                    (__compar_fn_t) bb_compare);
        if (bptr != NULL)
        {
            len = sprint_move(mvstr, &list.move[m]);
            switch (bptr->moveinfo)
            {
            case -1:
                annptr = "M"; /* merging into a position seen earlier */
                break;
            case 0:
                annptr = "";
                break;
            case 1:
                annptr = "!";
                break;
            case 2:
                annptr = "?";
                break;
            case 3:
                annptr = "!!";
                break;
            case 4:
                annptr = "??";
                break;
            case 5:
                annptr = "!?";
                break;
            case 6:
                annptr = "?!";
                break;
            default:
                sprintf(nagstr, "$%d", bptr->moveinfo);
                annptr = nagstr;
                break;
            }

            if (contin) /* continuing with next move in this subtree */
            {
                /* positioning at the proper column */
                printf("%*.*s", 8*ply, 8*ply, "\\"); 
            }
            contin = TRUE;

            /* print move without trailing space, then annotation */
            printf("%.*s%*s", len - 1, mvstr, len - 9, annptr);

            if (bptr->moveinfo == -1)
            {
                printf("\n");
                continue; /* subtree has merged, take next move in list */
            }

            /* recurse for next deeper level */
            dump_tree(&list.move[m], ply + 1);

            if (merge_dup)
            {
                bptr->moveinfo = -1; /* mark subtree as seen */
            }

        }
    }
    if (!contin) /* no book positions here, reached leaf of this subtree */
    {
        printf("\n");
    }
}

/* display opening book (or books) */
int main(int argc, char *argv[])
{
    int opt;
    char *bookfile;
    bitboard initbrd;

    while (TRUE)
    {
        opt = getopt(argc, argv, "dm");
        if (opt == -1)
        {
            break;
        }
        switch (opt)
        {
        case 'd':
            debug_info = TRUE;
            break;
        case 'm':
            merge_dup = TRUE;
            break;
        default:
            printf("Usage: %s [-d] [-m] bookfile...\n", argv[0]);
            printf("  -d = print lots of extra debug info\n"
                   "  -m = merge duplicate subtrees reached through transpositions\n"
                   "  bookfile = opening book to dump\n");
            exit(EXIT_FAILURE);
        }
    }

    init_board(&initbrd);
    while (optind < argc)
    {
        bookfile = argv[optind++];
        printf("processing %s\n", bookfile);
        init_book(bookfile);
        if (book_size == 0)
        {
            continue;
        }

        dump_tree(&initbrd, 0); /* start at the root node */
    }

    return EXIT_SUCCESS;
}
