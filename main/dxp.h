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

/* events that may terminate the search */
typedef union {
    struct {
        bool movenow : 1;
        bool connect : 1;
        bool gamereq : 1;
        bool move    : 1;
        bool gameend : 1;
        bool cmdexit : 1;
        bool cmdend  : 1;
    };
    bool any;
} mev;
extern mev main_event;

extern char engine_name[];
extern char opponent_name[];

extern int playtime_moves;   /* total nr. of moves for game */
extern int playtime_period;  /* time available for game, in milliseconds */
extern int timeleft_period;  /* remaining time for self, in ms */
extern int timeleft_opponent;/* remaining time for other player, in ms */
extern u32 move_time;        /* nominal think time for engine */
extern int test_depth;       /* max iterative search depth */
extern u32 test_time;        /* max search time per move (ms) */

extern int our_side;         /* engine's side in the game */
extern bool game_inprog;     /* game in progress */
extern int side_moving;      /* side to move in current root position */
extern int move_number;      /* the current move number */
extern int game_number;      /* game number in current connection */
extern int game_result;      /* from gameend */
extern char pdn_format[];    /* pdn log filename format */

extern bool verbose_info;    /* print verbose search info */

void poll_event(int wait);
