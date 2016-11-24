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

/* sizes.c: show the sizes of various structures */

#include "test.h"

typedef struct {
    char weight[4];
} featentry;
featentry feat[13];

int main(int argc, char *argv[])
{
    printf("sizeof(u8)=%u\n", (u32) sizeof(u8));
    printf("sizeof(u16)=%u\n", (u32) sizeof(u16));
    printf("sizeof(u32)=%u\n", (u32) sizeof(u32));
    printf("sizeof(u64)=%u\n", (u32) sizeof(u64));
    printf("sizeof(char)=%u\n", (u32) sizeof(char));
    printf("sizeof(short)=%u\n", (u32) sizeof(short));
    printf("sizeof(int)=%u\n", (u32) sizeof(int));
    printf("sizeof(long)=%u\n", (u32) sizeof(long));
    printf("sizeof(long long)=%u\n", (u32) sizeof(long long));
    printf("sizeof(bitboard)=%u\n", (u32) sizeof(bitboard));
    printf("sizeof(movelist)=%u\n", (u32) sizeof(movelist));
    printf("sizeof(ttentry)=%u\n", (u32) sizeof(ttentry));
    printf("sizeof(featentry)=%u\n", (u32) sizeof(featentry));
    printf("sizeof feat=%u\n", (u32) sizeof feat);
    printf("sizeof(endhf)=%u\n", (u32) sizeof(endhf));
    return EXIT_SUCCESS;
}
