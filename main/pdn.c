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

/* pdn.c: log the games as PDN */

#include "main.h"

bool pdnlog_inprog;           /* currently logging a game? */
bitboard pdn_board;           /* game's initial board state */
time_t pdn_start;             /* when the game started */
char pdn_movetext[20000];     /* storing the played moves as text */
char *pdn_moves;              /* where to write the next move text */

/* escape special characters */
/* dest -> destination string */
/* src -> source string */
static void add_escapes(char *dest, char *src)
{
    size_t d, s, len;

    /* copy while prepending \ before every \ and " */
    len = strlen(src);
    d = 0;
    for (s = 0; s < len; s++)
    {
        if (src[s] == '\\' || src[s] == '"')
        {
            dest[d++] = '\\';
        }
        dest[d++] = src[s];
    }
    dest[d++] = '\0';
}

/* strip trailing spaces */
/* in/out: str -> the string */
static void strip_trailing(char *str)
{
    size_t len;

    len = strlen(str);
    while (len > 0 && str[len - 1] == ' ')
    {
        len--;
    }
    str[len] = '\0';
}

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

/* log pdn move */
/* must be called before the move is performed */
/* listptr -> the list of valid moves, including the long move notation info */
/* mv = index number of the move in the move list */
void log_pdnmove(movelist *listptr, int mv)
{
    int m;

    if (pdn_moves == pdn_movetext && pdn_board.side != W)
    {
        pdn_moves += sprintf(pdn_moves, "\n1... ");
    }
    if (side_moving == W)
    {
        pdn_moves += sprintf(pdn_moves, "\n%d. ", move_number);
    }

    for (m = 0; m < listptr->count; m++)
    {
        if (m != mv &&
            move_square(&listptr->move[m], FROM) == 
            move_square(&listptr->move[mv], FROM) &&
            move_square(&listptr->move[m], TO) == 
            move_square(&listptr->move[mv], TO))
        {
            /* short move notation is ambiguous, using long notation */
            pdn_moves += sprint_move_long(pdn_moves, listptr, mv);
            return;
        }
    }
    /* we can use the short move notation, it is unambiguous */
    pdn_moves += sprint_move(pdn_moves, &listptr->move[mv]);
}

/* recursive part of finding an n-ballot opening */
/* startpos -> starting board position of game */
/* bb -> current board */
/* depth = how many moves left to check */
/* out: description of move */
/* returns: TRUE if starting board position reached */
static bool find_nextmove(bitboard *startpos, bitboard *bb, int depth, char *descr)
{
    movelist list;
    int m, len;

    if (depth == 0)
    {
        return bb_compare(startpos, bb) == EQUAL;
    }
    gen_moves(bb, &list, NULL, TRUE); /* generate all moves */
    for (m = 0; m < list.count; m++)
    {
        len = sprint_move(descr, &list.move[m]);
        if (find_nextmove(startpos, &list.move[m], depth - 1, descr + len))
        {                                                          /* recurse */
            return TRUE;
        }
    }
    return FALSE;
}

/* see if game started with an n-ballot opening */
/* startpos -> starting board position of game */
/* depth = how many moves to check */
/* out: description of opening (which may be a transposition) */
static void find_opening(bitboard *startpos, int depth, char *descr)
{
    bitboard brd;
    int n, len;

    init_board(&brd);
    len = sprintf(descr, "Opening ");
    /* iterate until max depth */
    for (n = 1; n <= depth; n++)
    {
        if (find_nextmove(startpos, &brd, n, descr + len))
        {
            return;
        }
    }
    strcpy(descr, "?");
}

/* do needed logging actions in case a game has started or stopped */
/* bb -> current board */
void log_pdnstartstop(bitboard *bb)
{
    FILE *fp;
    char pdnfile[PATH_MAX];
    char hostname[33];
    char eng[65], opp[65], nrmoves[10];
    char *result;
    char fen[210];
    struct tm *tmptr;
    int used;
    bitboard brd;

    if (game_inprog && !pdnlog_inprog)
    {
        /* game has just started */

        pdn_board = *bb;        /* save starting board state */
        pdn_start = time(NULL); /* save start date and time */
        pdn_moves = pdn_movetext;
        pdnlog_inprog = TRUE;
    }

    if (!game_inprog && pdnlog_inprog)
    {
        /* game has just stopped */

        sprintf(pdnfile, pdn_format, game_result); /* construct the filename */
        fp = fopen(pdnfile, "a");
        if (fp == NULL)
        {
            printf("can't open pdn logfile %s\n", pdnfile);
            pdnlog_inprog = FALSE;
            return;
        }

        gethostname(hostname, sizeof hostname);
        hostname[sizeof hostname - 1] = '\0';
        fprintf(fp, "\n[Site \"%s\"]\n", hostname);

        tmptr = localtime(&pdn_start);
        fprintf(fp, "[Date \"%04d.%02d.%02d\"]\n",
                tmptr->tm_year + 1900, tmptr->tm_mon + 1, tmptr->tm_mday);
        fprintf(fp, "[Time \"%02d:%02d:%02d\"]\n",
                tmptr->tm_hour, tmptr->tm_min, tmptr->tm_sec);

        fprintf(fp, "[Round \"%d\"]\n", game_number);

        add_escapes(eng, engine_name);
        strip_trailing(eng);
        add_escapes(opp, opponent_name);
        strip_trailing(opp);
        fprintf(fp, "[White \"%s\"]\n", (our_side == W) ? eng : opp);
        fprintf(fp, "[Black \"%s\"]\n", (our_side != W) ? eng : opp);

        switch (game_result)
        {
        case 1:            /* loss for engine */
            result = (our_side == W) ? "0-2" : "2-0";
            break;
        case 2:            /* draw */
            result = "1-1";
            break;
        case 3:            /* win for engine */
            result = (our_side != W) ? "0-2" : "2-0";
            break;
        case 0:            /* unknown */
        default:
            result = "*";
            break;
        }
        fprintf(fp, "[Result \"%s\"]\n", result);

        nrmoves[0] = '\0';
        if (playtime_moves > 0)
        {
            sprintf(nrmoves, "%d/", playtime_moves);
        }
        eng[0] = '\0';
        if (test_depth != 0 || test_time != 0) /* show test restrictions */
        {
            sprintf(eng, " { depth limit = %d, time limit = %dms/move }",
                    test_depth, test_time);
        }
        fprintf(fp, "[TimeControl \"%s%d\"]%s\n",
                nrmoves, playtime_period/1000, eng);

        /* time used is rounded down to whole seconds */
        used = (playtime_period - timeleft_period)/1000;
        sprintf(eng, "%d:%02d:%02d", used/3600, used/60%60, used%60);
        used = (playtime_period - timeleft_opponent)/1000;
        sprintf(opp, "%d:%02d:%02d", used/3600, used/60%60, used%60);
        fprintf(fp, "[WhiteTime \"%s\"]\n", (our_side == W) ? eng : opp);
        fprintf(fp, "[BlackTime \"%s\"]\n", (our_side != W) ? eng : opp);

        fprintf(fp, "[GameType \"20\"]\n");

        strcpy(eng, "?");
        init_board(&brd);
        if (bb_compare(&pdn_board, &brd) != EQUAL)
        {
            strcpy(fen, (pdn_board.side == W) ? "W:W" : "B:W");
            add_fen(fen, pdn_board.white, pdn_board.kings);
            strcat(fen, ":B");
            add_fen(fen, pdn_board.black, pdn_board.kings);
            fprintf(fp, "[FEN \"%s\"]\n", fen);

            find_opening(&pdn_board, 3, eng);
            strip_trailing(eng);
        }
        fprintf(fp, "[Event \"%s\"]\n", eng);

        pdn_moves += sprintf(pdn_moves, "\n%s\n\n", result);
        fwrite(pdn_movetext, sizeof(char), pdn_moves - pdn_movetext, fp);
        fclose(fp);
        pdnlog_inprog = FALSE;
    }
}
