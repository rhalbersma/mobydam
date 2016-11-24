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

extern u64 conv_to_bit(int square);
extern int conv_to_square(u64 pcbit);
extern void init_board(bitboard *bb);
extern void empty_board(bitboard *bb);
extern bool place_piece(bitboard *bb, int sq, int pc);
extern bool setup_fen(bitboard *bb, char *fen);
extern u64 move_captbits(bitboard *mvptr);
extern int move_square(bitboard *mvptr, int fromto);
extern int sprint_move(char *str, bitboard *mvptr);
extern void print_move(bitboard *mvptr);
extern int sprint_move_long(char *str, movelist *listptr, int m);
extern void print_move_long(movelist *listptr, int m);
extern void print_board(bitboard *bb);
extern bool is_draw(bitboard *startbb, int ply, char *descr);
extern int bb_compare(bitboard *bb1, bitboard *bb2);
extern void invert_board(bitboard *bb);
extern u32 get_tick(void);
extern char *locate_dbfile(char *dirs, char *name);
