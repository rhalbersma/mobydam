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
time_t pdn_start;             /* when the game started */

/* scratch variables for write_pdnmoves (to save stack space) */
movelist mv_list;             /* move list */
lnlist mv_ln;                 /* long notation */
char mv_str[210];             /* constructed string */

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

/* write moves to PDN file */
/* fp = file pointer */
/* bb -> current bitboard in move chain */
/* returns: move number of the last written move */
static int write_pdnmoves(FILE *fp, bitboard *bb)
{
    bitboard brd;
    bitboard *parent;
    int m, mv, movenr;
    bool uselong;

    parent = bb->parent;
    if (parent == NULL)
    {
        /* reached the end (initial position) of the move chain */
        init_board(&brd);
        if (bb_compare(bb, &brd) != EQUAL)
        {
            /* not the starting position, write FEN */
            strcpy(mv_str, (bb->side == W) ? "W:W" : "B:W");
            add_fen(mv_str, bb->white, bb->kings);
            strcat(mv_str, ":B");
            add_fen(mv_str, bb->black, bb->kings);
            fprintf(fp, "[FEN \"%s\"]\n", mv_str);

            /* see if it was a standard n-ballot opening */
            find_opening(bb, 3, mv_str);
            strip_trailing(mv_str);
        }
        else
        {
            strcpy(mv_str, "?");
        }
        fprintf(fp, "[Event \"%s\"]\n", mv_str);

        movenr = 0;
        if (bb->side != W)
        {
            fprintf(fp, "\n1... ");
            movenr = 1;
        }
    }
    else
    {
        /* follow the move chain to write the preceding moves */
        movenr = write_pdnmoves(fp, parent); /* recurse */

        /* write the move number */
        if (parent->side == W)
        {
            movenr++;
            fprintf(fp, "\n%d. ", movenr);
        }

        gen_moves(parent, &mv_list, &mv_ln, TRUE); /* generate all moves */
        uselong = FALSE;
        mv = 0;
        for (m = 0; m < mv_list.count; m++)
        {
            if (bb_compare(bb, &mv_list.move[m]) == EQUAL)
            {
                mv = m; /* save current move's index */
            }
            else if (move_square(bb, FROM) == move_square(&mv_list.move[m], FROM) &&
                     move_square(bb, TO)   == move_square(&mv_list.move[m], TO))
            {
                /* short move notation is ambiguous, using long notation */
                uselong = TRUE;
            }
        }
        if (uselong)
        {
            sprint_move_long(mv_str, &mv_list, mv);
        }
        else
        {
            sprint_move(mv_str, bb);
        }
        fprintf(fp, "%s", mv_str);
    }
    return movenr;
}

/* do needed logging actions in case a game has started or stopped */
/* bb -> current bitboard in move chain */
void log_pdnstartstop(bitboard *bb)
{
    FILE *fp;
    char pdnfile[PATH_MAX];
    char hostname[33];
    char eng[65], opp[65], nrmoves[10];
    char *result;
    struct tm *tmptr;
    int used;

    if (game_inprog && !pdnlog_inprog)
    {
        /* game has just started */

        pdn_start = time(NULL); /* save start date and time */
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

        write_pdnmoves(fp, bb);
        fprintf(fp, "\n%s\n\n", result);

        fclose(fp);
        pdnlog_inprog = FALSE;
    }
}
