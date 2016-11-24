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

#define DTWENDPC 4 /* max. piece count in exact dtw databases */
#define MAXENDPC 6 /* max. piece count in wdl databases */

#define EF 6                     /* endgame ref. table dimension per piece */

typedef struct {                 /* end game info file structure */
    off_t size;
    int   pccount;
    int   idx;
    u16   crc;
    char  name[14];
    int   matofs;
#ifdef _WIN32
    HANDLE hf;
    HANDLE hmap;
#else
    int   fd;
#endif
    u8    *fptr;
} endhf;

extern u64 end_acc[7];         /* access counts per piececount, and errors */

extern bool endgame_dtw(bitboard *bb, int ply, s32 *valp);
extern bool endgame_wdl(bitboard *bb, s32 *valp);
extern bool endgame_value(bitboard *bb, int ply, s32 *valp);
extern void check_enddb(void);
extern void init_enddb(char *dirs);
