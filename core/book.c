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

/* book.c: supply moves from opening book */

#include "core.h"

bitboard *book_positions = NULL;
size_t book_size;

/* prepare the book for use */
void init_book(char *bookfile)
{
    struct stat statbuf;
    FILE *fp;
    size_t size;
    int ret;

    book_size = 0;
    ret = stat(bookfile, &statbuf);
    if (ret != 0)
    {
        printf("init_book: can't find book file %s\n", bookfile);
        return;
    }
    fp = fopen(bookfile, "rb");
    if (fp == NULL)
    {
        printf("init_book: can't open book file %s\n", bookfile);
        return;
    }
    book_size = statbuf.st_size/sizeof(bitboard);
    if (book_size == 0)
    {
        printf("init_book: %s is an empty book file\n", bookfile);
        fclose(fp);
        return;
    }
    if (book_positions == NULL)
    {
        book_positions = malloc(book_size*sizeof(bitboard));
    }
    if (book_positions == NULL)
    {
        printf("init_book: can't allocate memory for book size = %u\n",
               (u32)book_size);
        fclose(fp);
        book_size = 0;
        return;
    }
    size = fread(book_positions, sizeof(bitboard), book_size, fp);
    fclose(fp);
    if (size != book_size)
    {
        printf("init_book: read %u instead of %u entries in book file %s\n",
                (u32)size, (u32)book_size, bookfile);
        book_size = 0;
        return;
    }
    printf("book positions = %u\n", (u32)book_size);
    return;
}

/* determine "weight" of move strength annotation */
/* annot = move strength value of considered move */
/* n = number of book moves to choose from */
/* returns: weight */
static int annot_weight(int annot, int n)
{
    switch (annot)
    {
    case 0:         /* no annotation */
        break;
    case 1:         /* good (!) */
        return n;
    case 2:         /* poor (?) */
    case 4:         /* very poor (??) */
        return 0;
    case 3:         /* very good (!!) is handled separately */
    default:
        break;
    }
    return 1;
}

/* get a book move */
/* listptr -> the list of valid moves */
/* returns: TRUE if book move selected; move is pulled to front of the list */
bool get_bookmove(movelist *listptr)
{
    bitboard *bptr;
    bitboard move;
    lnentry moveln;
    int m, n, x;

    if (book_size == 0)
    {
        return FALSE;
    }

    /* find current board position in the book */
    bptr = (bitboard *) bsearch(listptr->move[0].parent, book_positions,
                                book_size, sizeof(bitboard),
                                (__compar_fn_t) bb_compare);
    if (bptr == NULL)
    {
        return FALSE; /* not in book now */
    }
    n = 0;
    for (m = 0; m < listptr->count; m++)
    {
        /* find each move's board position in the book */
        bptr = (bitboard *) bsearch(&listptr->move[m], book_positions,
                                book_size, sizeof(bitboard),
                                (__compar_fn_t) bb_compare);
        if (bptr != NULL)
        {
            n++;      /* count valid moves that are present in book */
        }
    }
    if (n == 0)
    {
        return FALSE; /* none of the moves occur in book */
    }
    x = 0;
    for (m = 0; m < listptr->count; m++)
    {
        /* find each move's board position in the book */
        bptr = (bitboard *) bsearch(&listptr->move[m], book_positions,
                                book_size, sizeof(bitboard),
                                (__compar_fn_t) bb_compare);
        if (bptr != NULL)
        {
            if (bptr->moveinfo == 3)              /* very good (!!) move? */
            {
                break;                            /* always choose it */
            }
            x += annot_weight(bptr->moveinfo, n); /* weigh annotations */
        }
    }
    if (bptr == NULL || bptr->moveinfo != 3)
    {
        x = rand()%x;
        for (m = 0; m < listptr->count; m++)
        {
            /* find each move's board position in the book */
            bptr = (bitboard *) bsearch(&listptr->move[m], book_positions,
                                        book_size, sizeof(bitboard),
                                        (__compar_fn_t) bb_compare);
            if (bptr != NULL)
            {
                x -= annot_weight(bptr->moveinfo, n);
                if (x < 0)
                {
                    break;
                }
            }
        }
        if (x >= 0)
        {
            printf("get_bookmove: sanity check failed\n");
            return FALSE;
        }
    }

    /* find the selected book move in the list of valid moves */
    for (m = 0; m < listptr->count; m++)
    {
        if (bb_compare(&listptr->move[m], bptr) == EQUAL)
        {
#ifdef _DEBUG
            printf("book move is ");
            print_move(&listptr->move[m]);
            printf("\n");
#endif
            /* swap move to head of list */
            move = listptr->move[m];
            listptr->move[m] = listptr->move[0];
            listptr->move[0] = move;
            if (listptr->lnptr != NULL) /* long notation too, if present */
            {
                moveln = listptr->lnptr[m];
                listptr->lnptr[m] = listptr->lnptr[0];
                listptr->lnptr[0] = moveln;
            }
            return TRUE;
        }        
    }
    return FALSE;
}
