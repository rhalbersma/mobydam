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

/* util.c: various search-related functions and data */

#include "core.h"

#ifdef _WIN32
#define DIRSEP    ';'            /* separator for multiple db directories */
#define PATHDELIM '\\'           /* path delimiter */
#else
#define DIRSEP    ':'            /* separator for multiple db directories */
#define PATHDELIM '/'            /* path delimiter */
#endif

char db_path[PATH_MAX];          /* constructed path of database file */

/* bit positions 10, 21, 32, 43 are "ghost squares", enabling */
/* efficient move generation: diagonally adjacent squares */
/* are always shifts of -6/-5/+5/+6, independent of rank */

char bitpos2square[54] = { /* bit position to square number */
   1,  2,  3,  4,  5,
 6,  7,  8,  9, 10,    0,
  11, 12, 13, 14, 15,
16, 17, 18, 19, 20,    0,
  21, 22, 23, 24, 25,
26, 27, 28, 29, 30,    0,
  31, 32, 33, 34, 35,
36, 37, 38, 39, 40,    0,
  41, 42, 43, 44, 45,
46, 47, 48, 49, 50
};

u64 square2bitfield[51] = { 0, /* square number to bitfield */
  S01, S02, S03, S04, S05,
S06, S07, S08, S09, S10,
  S11, S12, S13, S14, S15,
S16, S17, S18, S19, S20,
  S21, S22, S23, S24, S25,
S26, S27, S28, S29, S30,
  S31, S32, S33, S34, S35,
S36, S37, S38, S39, S40,
  S41, S42, S43, S44, S45,
S46, S47, S48, S49, S50
};

/* convert square number to bitfield */
/* square = square number 1..50 */
/* returns: bitfield with corresponding bit set */
u64 conv_to_bit(int square)
{
    return square2bitfield[square];
}

/* convert bitfield to square number */
/* pcbit = bitfield with one bit set */
/* returns: corresponding square number 1..50 */
int conv_to_square(u64 pcbit)
{
    return bitpos2square[__builtin_ctzll(pcbit)];
}

/* set up initial position on the board */
/* out: bb -> board structure to fill */
void init_board(bitboard *bb)
{
    bb->white = ROW7 | ROW8 | ROW9 | ROW10; /* squares 31..50 */
    bb->black = ROW1 | ROW2 | ROW3 | ROW4;  /* squares  1..20 */
    bb->kings = 0;
    bb->side  = W;
    bb->moveinfo = 1; /* draw backstop */
    bb->parent = NULL;
}

/* set up an empty board */
/* out: bb -> board structure to fill */
void empty_board(bitboard *bb)
{
    bb->white = 0;
    bb->black = 0;
    bb->kings = 0;
    bb->side  = W;
    bb->moveinfo = 1; /* draw backstop */
    bb->parent = NULL;
}

/* place a piece on the board */
/* caller must place pieces on empty squares only */
/* bb -> board to modify */
/* sq = square number, 1..50 */
/* pc = piece type: MW, KW, MB, KB */
/* returns: FALSE if bad piece, bad square number or square occupied */
bool place_piece(bitboard *bb, int sq, int pc)
{
    u64 pcbit;

    if (sq < 1 || sq > 50)
    {
        return FALSE; /* invalid square number */
    }
    pcbit = conv_to_bit(sq);
    if ((pcbit & (bb->white | bb->black)) != 0)
    {
        return FALSE; /* square is not empty */
    }
    switch (pc)
    {
    case MW:
        bb->white |= pcbit;
        break;
    case KW:
        bb->white |= pcbit;
        bb->kings |= pcbit;
        break;
    case MB:
        bb->black |= pcbit;
        break;
    case KB:
        bb->black |= pcbit;
        bb->kings |= pcbit;
        break;
    default:
        return FALSE; /* not a valid piece */
    }
    return TRUE;
}

/* set up board according to FEN */
/* out: bb -> board structure to fill */
/* fen = pointer to FEN string */
/* returns: TRUE if FEN is valid */
bool setup_fen(bitboard *bb, char *fen)
{
    char *delimiter;
    int  pc, pos, nextpos;
    int toking[] = { KW, KW, KB, KB };
    int toman[] = { MW, MW, MB, MB };

    empty_board(bb);
    if (*fen == '\0')
    {
        fprintf(stderr, "No FEN present\n");
        return FALSE;
    }
    if (*fen == 'W')
    {
        bb->side = W;
    }
    else if (*fen == 'B')
    {
        bb->side = B;
    }
    else
    {
        fprintf(stderr, "Invalid FEN, side to move\n");
        return FALSE;
    }
    fen++;
    if (*fen != ':')
    {
        fprintf(stderr, "Invalid FEN, colon for piece list\n");
        return FALSE;
    }
    pc = pos = 0;
    do {
        fen++;
        if (*fen == 'W')
        {
            pc = MW;
        }
        else if (*fen == 'B')
        {
            pc = MB;
        }
        else
        {
            fprintf(stderr, "Invalid FEN, color of piece list\n");
            return FALSE;
        }
        do {
            delimiter = fen;
            fen++;
            if (*fen == 'K')
            {
                if (*delimiter == '-')
                {
                    fprintf(stderr, "Invalid FEN, range end\n");
                    return FALSE;
                }
                pc = toking[pc]; /* make it a king of the current color */
                fen++;
            }
            else if (*delimiter != '-')
            {
                pc = toman[pc]; /* make it a man of the current color */
            }
            nextpos = pos;
            pos = 0;
            while (*fen >= '0' && *fen <= '9')
            {
                pos = 10*pos + *fen++ - '0';
            }
            if (pos >= 1 && pos <= 50)
            {
                if ((pos <= 5 && pc == MW) || (pos >= 46 && pc == MB))
                {
                    fprintf(stderr, "Invalid FEN, unpromoted man\n");
                    return FALSE;
                }
                if (*delimiter == '-')  /* range */
                {
                    if (*fen == '-')
                    {
                        fprintf(stderr, "Invalid FEN, double range\n");
                        return FALSE;
                    }
                    if (nextpos > pos)
                    {
                        fprintf(stderr, "Invalid FEN, reverse range\n");
                        return FALSE;
                    }
                    while (++nextpos < pos)
                    {
                        if (!place_piece(bb, nextpos, pc))
                        {
                            fprintf(stderr, "Invalid FEN, duplicate square "
                                    "%d\n", nextpos);
                            return FALSE;
                        }
                    }
                }
                if (!place_piece(bb, pos, pc))
                {
                    fprintf(stderr, "Invalid FEN, duplicate square "
                            "%d\n", pos);
                    return FALSE;
                }
            }
            else
            {
                fprintf(stderr, "Invalid FEN, square number\n");
                return FALSE;
            }
        } while (*fen == ',' || *fen == '-');
    } while (*fen == ':');
    if (*fen != '\0' && *fen != '.' &&
        *fen != ' ' && *fen != '\r' && *fen != '\n')
    {
        fprintf(stderr, "Invalid FEN, unknown token '%c'\n", *fen);
        return FALSE;
    }
    return TRUE;
}

/* get a move's captured pieces */
/* a move is represented by its resulting bitboard; to find the */
/* captured pieces, compare new bitboard with parent bitboard */
/* mvptr -> move structure */
/* returns: bitfield of captured pieces */
u64 move_captbits(bitboard *mvptr)
{
    bitboard *bb;

    bb = mvptr->parent;
    if (bb->side == W)
    {
        return bb->black - mvptr->black;
    }
    else
    {
        return bb->white - mvptr->white;
    }
}

/* get a move's from- or to-square */
/* a move is represented by its resulting bitboard; to find the */
/* from- and to-squares, compare new bitboard with parent bitboard */
/* mvptr -> move structure */
/* fromto = FROM or TO */
/* returns: square number */
int move_square(bitboard *mvptr, int fromto)
{
    bitboard *bb;

    bb = mvptr->parent;
    if (bb->side == W)
    {
        if (bb->white == mvptr->white)
        {
            /* special case, from=to */
            return mvptr->moveinfo;
        }
        if (fromto == FROM)
        {
            return conv_to_square(bb->white & ~mvptr->white);
        }
        else
        {
            return conv_to_square(mvptr->white & ~bb->white);
        }
    }
    else
    {
        if (bb->black == mvptr->black)
        {
            /* special case, from=to */
            return mvptr->moveinfo;
        }
        if (fromto == FROM)
        {
            return conv_to_square(bb->black & ~mvptr->black);
        }
        else
        {
            return conv_to_square(mvptr->black & ~bb->black);
        }
    }
}

/* print move to string */
/* str -> destination string, must be at least 7 chars */
/* mvptr -> move structure */
/* returns: nr. of chars written, excluding nul */
int sprint_move(char *str, bitboard *mvptr)
{
    bitboard *bb;

    bb = mvptr->parent;
    if (bb->side == W)
    {
        return sprintf(str, "%d%c%d ",
                       move_square(mvptr, FROM),
                       (mvptr->black != bb->black) ? 'x' : '-',
                       move_square(mvptr, TO));
    }
    else
    {
        return sprintf(str, "%d%c%d ",
                       move_square(mvptr, FROM),
                       (mvptr->white != bb->white) ? 'x' : '-',
                       move_square(mvptr, TO));
    }
}

/* print move */
/* mvptr -> move structure */
void print_move(bitboard *mvptr)
{
    char mvstr[7];

    sprint_move(mvstr, mvptr);
    fputs(mvstr, stdout);
}

/* print move in long notation to string */
/* str -> destination string, must be at least 94 chars */
/* listptr -> the list of valid moves; the long move */
/*            notation info should be available;     */
/*            if not, fallback to the short notation */
/* m = index number of the move in the move list */
/* returns: nr. of chars written, excluding nul */
int sprint_move_long(char *str, movelist *listptr, int m)
{
    int n, len;

    if (listptr->lnptr != NULL)
    {

        len = sprintf(str, "%d", listptr->lnptr[m].square[0]);
        for (n = 1; listptr->lnptr[m].square[n] != 0; n++)
        {
            len += sprintf(&str[len], "%c%d",
                           (listptr->npcapt != 0) ? 'x' : '-',
                           listptr->lnptr[m].square[n]);
        }
        len += sprintf(&str[len], " ");
    }
    else
    {
        len = sprint_move(str, &listptr->move[m]);
    }
    return len;
}

/* print move in long notation */
/* listptr -> the list of valid moves; the long move */
/*            notation info should be available;     */
/*            if not, fallback to the short notation */
/* m = index number of the move in the move list */
void print_move_long(movelist *listptr, int m)
{
    char mvstr[94];

    sprint_move_long(mvstr, listptr, m);
    fputs(mvstr, stdout);
}

/* print diagram of board in ascii graphics */
/* bb -> current board */
void print_board(bitboard *bb)
{
    u64 pcbit;
    int x, y;

    for (y = 0; y < 10; y++)
    {
        if (y%2 == 0)
        {
            printf("  ");
        }
        for (x = 1; x <= 5; x++)
        {
            pcbit = conv_to_bit(5*y + x);
            if ((bb->white & pcbit) != 0)
            {
                printf("   %c", ((bb->kings & pcbit) != 0) ? 'W' : 'w');
            }
            else if ((bb->black & pcbit) != 0)
            {
                printf("   %c", ((bb->kings & pcbit) != 0) ? 'B' : 'b');
            }
            else
            {
                printf("   .");
            }
        }
        if (y == 9)
        {
            printf("   (%c to move)", (bb->side == W) ? 'w' : 'b');
        }
        printf("\n");
    }
}

/* determine draw by repetition or aimless king moves, using rules from */
/* KNDB Handboek Spel- en Wedstrijdreglement (maart 2013) Artikel 9. */
/* startbb -> current board, start of chain to follow */
/* ply = current search ply depth, 0 = not in search */
/* out: descr -> string to receive description of the applicable draw rule, */
/*               or NULL */
bool is_draw(bitboard *startbb, int ply, char *descr)
{
    bitboard *bb;
    int i, m, r;

    bb = startbb;
    if (ply == 0 && popcount(bb->white | bb->black) <= 4)
    {
        /* check lone king against 1, 2 or 3 pieces including a king. */
        /* no need to do this while searching, the dtw endgame database */
        /* already takes care of finishing a winning sequence before */
        /* this draw rule kicks in. */

        /* step back 5 moves (10 half-moves) */
        for (i = 0; i < 10; i++)
        {
            bb = bb->parent;
            if (bb == NULL)
            {
                break;
            }
        }
        if (bb != NULL)
        {
            if ((bb->white & bb->kings) != 0 &&
                (bb->black & bb->kings) != 0 &&
                ((popcount(bb->white) == 1 && popcount(bb->black) <= 2) ||
                 (popcount(bb->black) == 1 && popcount(bb->white) <= 2)))
            {
                if (descr != NULL)
                {
                    sprintf(descr, 
                            "%d moves since position arose with 1 king "
                            "against 1 or 2 pieces including a king (9b)", i/2);
                }
                return TRUE;
            }

            /* step back another 11 moves for a total of 16 (32 half-moves) */
            for (; i < 32; i++)
            {
                bb = bb->parent;
                if (bb == NULL)
                {
                    break;
                }
            }
            if (bb != NULL)
            {
                if ((bb->white & bb->kings) != 0 &&
                    (bb->black & bb->kings) != 0 &&
                    ((popcount(bb->white) == 1 && popcount(bb->black) == 3) ||
                     (popcount(bb->black) == 1 && popcount(bb->white) == 3)))
                {
                    if (descr != NULL)
                    {
                        sprintf(descr, 
                                "%d moves since position arose with 1 king "
                                "against 3 pieces including a king (9c)", i/2);
                    }
                    return TRUE;
                }
            }
        }
    }

    /* check for repetitions and the 25-move rule */

    bb = startbb;
    m = 0;
    r = 0;
    do {
        if (bb->moveinfo != 0) /* stop at capture or man move */
        {                      /* (is always set in final board of chain) */
            return FALSE;
        }
        bb = bb->parent;       /* now seeing previous opponent's move */
        if (bb->moveinfo != 0) /* stop at capture or man move */
        {                      /* (is always set in final board of chain) */
            return FALSE;
        }
        bb = bb->parent;       /* now seeing previous own move */
        m++;                   /* count the whole-moves */
        if (bb->white == startbb->white &&
            bb->black == startbb->black &&
            bb->kings == startbb->kings)
        {
            /* while searching beyond the first ply, */
            /* avoid even one repetition (when ahead) */
            if (ply > 1)
            {
                if (descr != NULL)
                {
                    sprintf(descr, "same position as %d moves ago", m);
                }
                return TRUE;
            }
            if (r != 0) /* saw an earlier repeat */
            {
                /* two earlier identical positions found */
                if (descr != NULL)
                {
                    sprintf(descr, "same position as %d and %d moves ago (9e)",
                            r, m);
                }
                return TRUE;
            }
            r = m; /* save the first repeat */
        }
    } while (m < 25); /* step back upto 25 moves */

    if (descr != NULL)
    {
        sprintf(descr, "%d moves without capture or man move (9d)", m);
    }
    return TRUE;
}

/* compare two bitboards */
/* bb1 -> bitboard 1 */
/* bb2 -> bitboard 2 */
/* returns:   0 (EQUAL) = identical */
/*          < 0 = bb1 < bb2 */
/*          > 0 = bb1 > bb2 */
int bb_compare(bitboard *bb1, bitboard *bb2)
{
    if (bb1->white < bb2->white)
    {
        return -1;
    }
    if (bb1->white > bb2->white)
    {
        return 1;
    }
    if (bb1->black < bb2->black)
    {
        return -1;
    }
    if (bb1->black > bb2->black)
    {
        return 1;
    }
    if (bb1->kings < bb2->kings)
    {
        return -1;
    }
    if (bb1->kings > bb2->kings)
    {
        return 1;
    }
    if (bb1->side < bb2->side)
    {
        return -1;
    }
    if (bb1->side > bb2->side)
    {
        return 1;
    }
    return EQUAL;
}

/* reverse colors & sides */
/* in/out: bb -> board to invert */
void invert_board(bitboard *bb)
{
    u64 white, black, kings, mask;

    white = bb->white;
    black = bb->black;
    kings = bb->kings;
    mask = 0x5555555555555555ULL;
    white = ((white >> 1) & mask) | ((white & mask) << 1);
    black = ((black >> 1) & mask) | ((black & mask) << 1);
    kings = ((kings >> 1) & mask) | ((kings & mask) << 1);
    mask = 0x3333333333333333ULL;
    white = ((white >> 2) & mask) | ((white & mask) << 2);
    black = ((black >> 2) & mask) | ((black & mask) << 2);
    kings = ((kings >> 2) & mask) | ((kings & mask) << 2);
    mask = 0x0f0f0f0f0f0f0f0fULL;
    white = ((white >> 4) & mask) | ((white & mask) << 4);
    black = ((black >> 4) & mask) | ((black & mask) << 4);
    kings = ((kings >> 4) & mask) | ((kings & mask) << 4);
    bb->white = __builtin_bswap64(black) >> 10;
    bb->black = __builtin_bswap64(white) >> 10;
    bb->kings = __builtin_bswap64(kings) >> 10;
    bb->side = W + B - bb->side;
}

/* give current timestamp in milliseconds */
/* note: 32-bit wrap-around is not a problem as long as */
/* callers calculate the interval between two timestamps */
/* using unsigned arithmetic. */
/* returns: timestamp */
u32 get_tick(void)
{
#ifdef _WIN32
    return timeGetTime();
#else
    struct timeval tv;

    if (gettimeofday(&tv, NULL) == -1)
    {
        perror("get_tick");
        return 0;
    }
    return tv.tv_sec*1000 + tv.tv_usec/1000;
#endif
}

/* get database directory */
/* dirs = directory path to search */
/* section = which part of (semi)colon-separated path to select */
/* returns: TRUE = requested part copied to db_path */
/*          FALSE = requested part not found        */
static bool get_dbdir(char *dirs, int section)
{
    char *dbptr, *sepptr;
    size_t len;

    dbptr = dirs;
    while (section > 0) /* locate requested section */
    {
        dbptr = strchr(dbptr, DIRSEP);
        if (dbptr == NULL)
        {
            return FALSE;
        }
        dbptr++;
        section--;
    }
    sepptr = strchr(dbptr, DIRSEP); /* remove successive sections */
    if (sepptr != NULL)
    {
        len = sepptr - dbptr;
    }
    else
    {
        len = strlen(dbptr);
    }
    strncpy(db_path, dbptr, len);
    if (db_path[len - 1] != PATHDELIM)
    {
        db_path[len++] = PATHDELIM; /* final slash */
    }
    db_path[len] = '\0';
    return TRUE;
}

/* locate database file */
/* dirs = directory/ies to search */
/* name = basename of desired file */
/* returns: the path, or NULL if not found */
char *locate_dbfile(char *dirs, char *name)
{
    struct stat statbuf;
    int  section;
    int  ret;

    section = 0;
    do {
        if (!get_dbdir(dirs, section)) /* try next dir in db path */
        {
            return NULL;
        }
        strcat(db_path, name);         /* construct the path */
        ret = stat(db_path, &statbuf);
        section++;
    } while (ret == -1);
    return db_path;
}
