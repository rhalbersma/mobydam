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

/* bookgen.c: create new or add to existing opening book */

#include "test.h"

#define BOOKSIZE 1048576 /* 2^20 */
#define PDNLEN 256

bool debug_info = FALSE;
bool annot_ignore = FALSE;
bool annot_overwrite = FALSE;
char book_file[PATH_MAX] = "book.opn"; /* opening book filename */
size_t book_newsize;
int book_depth = 20;            /* max. depth of game moves to add to book */

char pdn_event[PDNLEN];         /* pdn seven-tag roster */
char pdn_site[PDNLEN];
char pdn_date[PDNLEN];
char pdn_round[PDNLEN];
char pdn_white[PDNLEN];
char pdn_black[PDNLEN];
char pdn_result[PDNLEN];
int pdn_numresult;
char pdn_setup[PDNLEN];         /* setup tag */
char pdn_fen[PDNLEN];           /* fen tag */
char *pdn_val[] = { pdn_event, pdn_site, pdn_date, pdn_round, pdn_white,
                    pdn_black, pdn_result, pdn_setup, pdn_fen };
char *pdn_tag[] = { "Event", "Site", "Date", "Round", "White",
                    "Black", "Result", "SetUp", "FEN" };
char *str_result[] =
{ "1-0", "0-1", "1/2-1/2", "*", "2-0", "0-2", "1-1", "0-0", "" };
char pdn_token[PDNLEN];         /* current pdn token */
char *pdn_text = NULL;          /* the pdn text */
u64 pdn_size;                   /* the pdn text length */
char *pdn_ptr = NULL;           /* current position in pdn text */
int pdn_pos;                    /* character position within current pdn line */

/* add a move to the book */
/* mvptr -> move to add */
/* nagcode = move's annotation */
/* returns: TRUE if successful */
static bool add_bookmove(bitboard *mvptr, int nagcode)
{
    char mvstr[7];
    bitboard *bptr;
    movelist list;
    size_t b;
    int m;

    /* occasionally update sorted book position array for search efficiency */
    if (book_newsize >= book_size + 3000)
    {
        debugf("re-sorting\n");
        book_size = book_newsize;
        qsort(book_positions, book_size, sizeof(bitboard),
              (__compar_fn_t) bb_compare);
    }

    /* find the move's board position in the book */
    bptr = (bitboard *) bsearch(mvptr, book_positions, book_size,
                                sizeof(bitboard), (__compar_fn_t) bb_compare);
    if (bptr == NULL)
    {
        /* do linear search among recently added moves */
        for (b = book_size; b < book_newsize; b++)
        {
            if (bb_compare(mvptr, &book_positions[b]) == EQUAL)
            {
                bptr = &book_positions[b];
                break;
            }
        }
    }

    if (bptr == NULL) /* move does not yet exist in book, create it */
    {
        if (book_newsize >= BOOKSIZE)
        {
            printf("add_bookmove: book is full\n");
            return FALSE;
        }
        bptr = &book_positions[book_newsize];
        *bptr = *mvptr;
        bptr->moveinfo = 0;
        bptr->parent = NULL;
        book_newsize++;
        debugf("new move added ");
    }
    else
    {
        debugf("existing move found ");
    }

    sprint_move(mvstr, mvptr);
    debugf("%s annot %d->%d\n", mvstr, bptr->moveinfo, nagcode);

    if (annot_ignore || nagcode == 0)
    {
        return TRUE;
    }

    if (nagcode == 3) /* (!!) means to always force selection of this move */
    {
        /* check that no alternative move is forced */
        gen_moves(mvptr->parent, &list, NULL, TRUE);
        for (m = 0; m < list.count; m++)
        {
            /* find alternative move's board position in the book */
            mvptr = (bitboard *) bsearch(&list.move[m], book_positions,
                                         book_size, sizeof(bitboard),
                                         (__compar_fn_t) bb_compare);
            if (mvptr == NULL)
            {
                /* do linear search among recently added moves */
                for (b = book_size; b < book_newsize; b++)
                {
                    if (bb_compare(&list.move[m], &book_positions[b]) == EQUAL)
                    {
                        mvptr = &book_positions[b];
                        break;
                    }
                }
            }
            if (mvptr != NULL && bb_compare(mvptr, bptr) != EQUAL &&
                mvptr->moveinfo == 3)
            {
                if (annot_overwrite)
                {
                    debugf("clearing conflicting forced move annot 3\n");
                    mvptr->moveinfo = 0;
                }
                else
                {
                    debugf("conflicting forced move annot 3 found\n");
                    return TRUE; /* don't annotate our newly added move */
                }
            }
        }
    }
    if (annot_overwrite || bptr->moveinfo == 0)
    {
        bptr->moveinfo = nagcode;
    }
    return TRUE;
}

/* read character from pdn input buffer */
/* returns: != 0 = character             */
/*          == 0 = end of buffer reached */
static char read_char(void)
{
    char ch;

    if (pdn_ptr >= pdn_text + pdn_size)
    {
        return '\0';
    }
    ch = *pdn_ptr++;
    if (ch == '\0')
    {
        ch = ' ';
    }
    if (ch == '\r' || ch == '\n')
    {
        pdn_pos = 0;
    }
    else
    {
        pdn_pos++;
    }
    return ch;
}

/* back up position in pdn input buffer */
static void put_char(void)
{
    pdn_ptr--;
    if (pdn_pos > 0)
    {
        pdn_pos--;
    }
}

/* test if character is whitespace */
/* ch = the character */
/* returns: TRUE  = whitespace */
/*          FALSE = other      */
static bool is_white(char ch)
{
    switch (ch)
    {
    case ' ':
    case ',':
    case '\t':
    case '\n':
    case '\f':
    case '\r':
        return TRUE;
    default:
        break;
    }
    return FALSE;
}

/* test if character is part of symbol token */
/* ch = the character */
/* returns: TRUE  = symbol char */
/*          FALSE = other       */
static bool is_symbol(char ch)
{
    if (ch >= 'A' && ch <= 'Z')
    {
        return TRUE;
    }
    if (ch >= 'a' && ch <= 'z')
    {
        return TRUE;
    }
    if (ch >= '0' && ch <= '9')
    {
        return TRUE;
    }
    switch (ch)
    {
    case '_':
    case '+':
    case '#':
    case '=':
    case ':':
    case '-':
    case '/':
        return TRUE;
    default:
        break;
    }
    return FALSE;
}

/* read token from pdn input buffer */
/* returns: TRUE  = success               */
/*          FALSE = end of buffer reached */
static bool read_tok(void)
{
    char ch;
    int  i;

    i = 0;
    pdn_token[0] = '\0';
    do {                                /* skip white space */
        ch = read_char();
    } while (is_white(ch));
    if (ch == '\0')
    {
        return FALSE;
    }
    if (ch == '"')                      /* string token */
    {
        do {
            ch = read_char();
            switch (ch)
            {
            case '\0':                  /* end of string */
            case '"':
            case '\r':
            case '\n':
                break;
            case '\\':                  /* quote character */
                ch = read_char();
                if (ch == '\0' || ch == '\r' || ch == '\n')
                {
                    break;
                }
                if (i < PDNLEN - 1)
                {
                    pdn_token[i++] = ch;
                }
                continue;
            default:                    /* regular character */
                if (i < PDNLEN - 1)
                {
                    pdn_token[i++] = ch;
                }
                continue;
            }
            break;
        } while (TRUE);
    }
    else if (ch == ';' || (pdn_pos == 1 && ch == '%'))
    {                                   /* comment to end of line */
        do {
            ch = read_char();
        } while (ch != '\0' && ch != '\r' && ch != '\n');
        return read_tok();
    }
    else if (ch == '{')                 /* comment in braces, not recursive */
    {
        do {
            ch = read_char();
        } while (ch != '\0' && ch != '}');
        return read_tok();
    }
    else if (ch == '(')                 /* recursive annotation variations */
    {
        do {
            if (!read_tok())
            {
                return FALSE;
            }
        } while (strcmp(pdn_token, ")") != EQUAL);
        return read_tok();
    }
    else if (ch == '<')                 /* reserved <>, assume recursive */
    {
        do {
            if (!read_tok())
            {
                return FALSE;
            }
        } while (strcmp(pdn_token, ">") != EQUAL);
        return read_tok();
    }
    else if (ch == '$')                 /* numeric annotation (NAG) token */
    {
        do {
            pdn_token[i++] = ch;
            ch = read_char();
        } while (ch >= '0' && ch <= '9');
        put_char();
    }
    else if (ch == '!')                 /* traditional suffix annotation */
    {
        pdn_token[i++] = '$';           /* convert to NAG code */
        pdn_token[i] = '1';             /* good move */
        ch = read_char();
        if (ch == '!')
        {
            pdn_token[i] = '3';         /* very good move */
        }
        else if (ch == '?')
        {
            pdn_token[i] = '5';         /* speculative move */
        }
        else
        {
            put_char();
        }
        i++;
    }
    else if (ch == '?')                 /* traditional suffix annotation */
    {
        pdn_token[i++] = '$';           /* convert to NAG code */
        pdn_token[i] = '2';             /* poor move */
        ch = read_char();
        if (ch == '!')
        {
            pdn_token[i] = '6';         /* questionable move */
        }
        else if (ch == '?')
        {
            pdn_token[i] = '4';         /* very poor move */
        }
        else
        {
            put_char();
        }
        i++;
    }
    else                                /* regular token */
    {
        pdn_token[i++] = ch;
        while (is_symbol(ch))
        {
            ch = read_char();
            if (is_symbol(ch))
            {
                if (i < PDNLEN - 1)
                {
                    pdn_token[i++] = ch;
                }
            }
            else
            {
                put_char();
            }
        }
    }
    pdn_token[i] = '\0';
    return TRUE;
}

/* read pdn header from input buffer */
/* returns: TRUE if successful */
static bool read_pdnhdr(void)
{
    int i;

    while (pdn_token[0] != '[')
    {                                   /* find first tag section */
        if (!read_tok())
        {
            return FALSE;
        }
    }                                   /* clear all tag values */
    for (i = 0; i < elements(pdn_val); i++)
    {
        memset(pdn_val[i], 0, PDNLEN*sizeof(char));
    }
    do {
        if (!read_tok())                /* read tag */
        {
            return FALSE;
        }
        for (i = 0; i < elements(pdn_val); i++)
        {                               /* case insensitive compare */
            if (strcasecmp(pdn_token, pdn_tag[i]) == EQUAL)
            {
                if (!read_tok())        /* next token is tag value */
                {
                    return FALSE;
                }
                strncpy(pdn_val[i], pdn_token, PDNLEN - 1);
                break;
            }
        }
        do {
            if (!read_tok())            /* find end bracket */
            {
                return FALSE;
            }
        } while (strcmp(pdn_token, "]") != EQUAL);
        if (!read_tok())                /* possible subsequent start bracket */
        {
            return FALSE;
        }
    } while (strcmp(pdn_token, "[") == EQUAL);
    return TRUE;
}

/* process the pdn movetext section */
/* returns: TRUE if successful */
static bool load_moves(void)
{
    char *ptr;
    int pos, m, fromto, nagcode, depth;
    int move[2];
    bitboard brd;
    movelist list;

    depth = 0;
    init_board(&brd);
    while (depth < book_depth &&
           pdn_token[0] != '\0'&&
           strcmp(pdn_token, "[") != EQUAL)
    {   /* stop at any of the standard or not-so-standard result codes */
        for (m = 0; m < elements(str_result) - 1; m++)
        {
            if (strcmp(pdn_token, str_result[m]) == EQUAL)
            {
                return TRUE;
            }
        }

        /* extract first and last square number from token */
        /* ignoring any disambiguation squares */
        ptr = pdn_token;
        move[FROM] = move[TO] = 0;
        fromto = FROM;
        nagcode = 0;
        do {
            while (*ptr != '\0' && (*ptr < '0' || *ptr > '9'))
            {
                ptr++;
            }
            if (*ptr != '\0')
            {
                pos = 0;
                while (*ptr >= '0' && *ptr <= '9')
                {
                    pos = 10*pos + *ptr++ - '0';
                }
                move[fromto] = pos;
                fromto = TO;
            }
        } while (*ptr != '\0');
        if (read_tok())
        {
            if (move[TO] == 0 &&                 /* single number followed by */
                strcmp(pdn_token, ".") == EQUAL) /* a period is a move number */
            {
                if (!read_tok())
                {
                    break;
                }
                continue;           /* go on with token following period */
            }
            if (pdn_token[0] == '$')             /* extract nag code */
            {
                nagcode = atoi(pdn_token + 1);
                if (!read_tok())
                {
                    break;
                }
            }
        }

        /* find pdn move among the valid moves */
        gen_moves(&brd, &list, NULL, TRUE); /* generate all moves */
        for (m = 0; m < list.count; m++)
        {
            if (move_square(&list.move[m], FROM) == move[FROM] &&
                move_square(&list.move[m], TO) == move[TO])
            {
                break;
            }
        }
        if (m >= list.count)
        {
            printf("load_moves: invalid pdn move\n");
            return FALSE;
        }

        /* add this move to the book */
        if (!add_bookmove(&list.move[m], nagcode))
        {
            return FALSE;
        }

        brd = list.move[m];
        depth++;
    }
    return TRUE;
}

/* load contents of pdn file into memory */
/* pdnfile = name of the file to load into pdn_text */
/* returns: TRUE if successful */
static bool load_pdn(char *pdnfile)
{
    struct stat statbuf;
    FILE *fp;
    int ret;

    ret = stat(pdnfile, &statbuf);
    if (ret != 0)
    {
        printf("load_pdn: can't find pdn file %s\n", pdnfile);
        return FALSE;
    }
    fp = fopen(pdnfile, "rb");
    if (fp == NULL)
    {
        printf("load_pdn: can't open pdn file %s\n", pdnfile);
        return FALSE;
    }
    free(pdn_text);
    pdn_text = malloc(statbuf.st_size*sizeof(char));
    if (pdn_text == NULL)
    {
        printf("load_pdn: can't allocate %" PRIu64
               " bytes of memory for pdn file %s\n",
               (u64)statbuf.st_size, pdnfile);
        fclose(fp);
        return FALSE;
    }
    pdn_size = fread(pdn_text, sizeof(char), statbuf.st_size, fp);
    fclose(fp);
    if (pdn_size != statbuf.st_size)
    {
        printf("load_pdn: read %" PRIu64 " instead of %" PRIu64
               " chars in pdn file %s\n",
               pdn_size, (u64)statbuf.st_size, pdnfile);
        return FALSE;
    }
    pdn_ptr = pdn_text;
    pdn_token[0] = '\0';
    pdn_pos = 0;
    return TRUE;
}

/* save the new book to file */
/* bookfile = book filename */
static void save_book(char *bookfile)
{
    FILE *fp;
    size_t size;

    fp = fopen(bookfile, "wb");
    if (fp == NULL)
    {
        printf("save_book: can't open book file %s for writing\n", bookfile);
        return;
    }
    qsort(book_positions, book_newsize, sizeof(bitboard),
          (__compar_fn_t) bb_compare);
    size = fwrite(book_positions, sizeof(bitboard), book_newsize, fp);
    fclose(fp);
    if (size != book_newsize)
    {
        printf("save_book: can't write %u of %u entries to book file %s\n",
               (u32)(book_newsize - size), (u32)book_newsize, bookfile);
        return;
    }
    debugf("written %u entries to book file %s\n", (u32)book_newsize, book_file);
}

/* assemble pdn games into opening book */
int main(int argc, char *argv[])
{
    int opt, i;
    char *pdnfile;
    bitboard brd, initbrd;

    while (TRUE)
    {
        opt = getopt(argc, argv, "b:diol:");
        if (opt == -1)
        {
            break;
        }
        switch (opt)
        {
        case 'b':
            strncpy(book_file, optarg, sizeof book_file - 1);
            break;
        case 'd':
            debug_info = TRUE;
            break;
        case 'i':
            annot_ignore = TRUE;
            break;
        case 'o':
            annot_overwrite = TRUE;
            break;
        case 'l':
            book_depth = atoi(optarg);
            break;
        default:
            printf("Usage: %s [-b bookfile] [-d] [-i] [-o] [-l depth] "
                   "pdnfiles...\n", argv[0]);
            printf("  -b bookfile = file name of opening book\n"
                   "       (default: book.opn)\n"
                   "  -d = print lots of extra debug info\n"
                   "  -i = ignore annotations in pdnfiles\n"
                   "  -o = overwrite book annotations by pdnfiles annotations\n"
                   "  -l depth = limit to ply depth of moves to add\n"
                   "       (default: 20 ply = 10 moves/side)\n"
                   "  pdnfiles = set of games to add to opening book\n");
            exit(EXIT_FAILURE);
        }
    }

    /* allocate the max to book array */
    book_positions = malloc(BOOKSIZE*sizeof(bitboard));
    if (book_positions == NULL)
    {
        printf("bookgen: can't allocate memory for book\n");
        exit(EXIT_FAILURE);
    }

    init_board(&initbrd);
    init_book(book_file); /* if book file exists read it in */
    book_newsize = book_size;
    if (book_size == 0)
    {
        printf("creating new book file %s\n", book_file);
        /* the starting position is the root node of the book */
        book_positions[0] = initbrd;
        book_positions[0].moveinfo = 0; /* annot */
        book_positions[0].parent = NULL;
        book_newsize = 1;
    }

    while (optind < argc)
    {
        pdnfile = argv[optind++];
        if (!load_pdn(pdnfile))
        {
            exit(EXIT_FAILURE);
        }
        printf("processing %s\n", pdnfile);
        i = 0;
        while (read_pdnhdr())
        {
            i++;
            /* an opening needs no fen, but if a fen is present, */
            /* we expect to see only the starting position */
            if (pdn_fen[0] != '\0')
            {
                if (!setup_fen(&brd, pdn_fen) ||
                    bb_compare(&brd, &initbrd) != EQUAL)
                {
                    printf("skipping %s game %d, not at starting position\n",
                           pdnfile, i);
                    continue;
                }
            }
            debugf("adding %s game %d\n", pdnfile, i);
            if (!load_moves())
            {
                printf("error while processing %s game %d\n", pdnfile, i);
                /* stop processing remaining pdn games/files */
                optind = argc;
                break;
            }
        }
    }
    debugf("newsize = %u\n", (u32)book_newsize);
    save_book(book_file);
    return EXIT_SUCCESS;
}
