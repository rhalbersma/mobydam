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

/* end.c: endgame database access functions */

#include "core.h"

u64 end_acc[7];                  /* access statistics */
endhf *end_ref[EF*EF*EF*EF];     /* references to endgame file info */
u32 combi_array[51][8];          /* combination lookup table */
char enddb_dirs[PATH_MAX];       /* directory/ies of database files */

/* endgame files, with size, piece count, table index, CRC, name */
endhf end_set[] = {
  {    2500, 2, -1, 0xd2d8, "OvO.bin" },
  {    2500, 2, -1, 0x6915, "XvO.bin" },
  {    2500, 2, -1, 0xb1a5, "OvX.bin" },
  {    2500, 2, -1, 0x7585, "XvX.bin" },
  {  125000, 3, -1, 0x9965, "OOvO.bin" },
  {  125000, 3, -1, 0x3184, "OOvX.bin" },
  {  125000, 3, -1, 0x13ea, "OvOO.bin" },
  {  125000, 3, -1, 0xec9f, "OvXO.bin" },
  {  125000, 3, -1, 0x74bd, "OvXX.bin" },
  {  125000, 3, -1, 0xf732, "XOvO.bin" },
  {  125000, 3, -1, 0x6cab, "XOvX.bin" },
  {  125000, 3, -1, 0x9752, "XvOO.bin" },
  {  125000, 3, -1, 0xa0f7, "XvXO.bin" },
  {  125000, 3, -1, 0xf548, "XvXX.bin" },
  {  125000, 3, -1, 0xc2ae, "XXvO.bin" },
  {  125000, 3, -1, 0x703d, "XXvX.bin" },
  {  347326, 4,  0, 0x9444, "OOOvO.cpr" },
  {  185557, 4,  1, 0x8659, "OOOvX.cpr" },
  {  431432, 4,  2, 0xf6fb, "OOvOO.cpr" },
  {  960067, 4,  3, 0x6fe2, "OOvXO.cpr" },
  {  648003, 4,  4, 0x369a, "OOvXX.cpr" },
  {  280344, 4,  5, 0xc0ee, "OvOOO.cpr" },
  { 1027656, 4,  6, 0x45d1, "OvXOO.cpr" },
  { 1324595, 4,  7, 0x5b3d, "OvXXO.cpr" },
  {  508845, 4,  8, 0x5897, "OvXXX.cpr" },
  { 1369691, 4,  9, 0x85d6, "XOOvO.cpr" },
  {  960613, 4, 10, 0xec0b, "XOOvX.cpr" },
  { 1484517, 4, 11, 0x4648, "XOvOO.cpr" },
  { 1530033, 4, 12, 0xe1e6, "XOvXO.cpr" },
  {  357807, 4, 13, 0x11e1, "XOvXX.cpr" },
  {  298268, 4, 14, 0xdc0d, "XvOOO.cpr" },
  {  517263, 4, 15, 0xe205, "XvXOO.cpr" },
  {  375668, 4, 16, 0xd23b, "XvXXO.cpr" },
  {  128906, 4, 17, 0x5695, "XvXXX.cpr" },
  { 1579352, 4, 18, 0xdf97, "XXOvO.cpr" },
  { 1339555, 4, 19, 0x1584, "XXOvX.cpr" },
  {  862757, 4, 20, 0x05d3, "XXvOO.cpr" },
  { 1180204, 4, 21, 0x8282, "XXvXO.cpr" },
  {  262388, 4, 22, 0xc7a1, "XXvXX.cpr" },
  {  559505, 4, 23, 0x4d97, "XXXvO.cpr" },
  {  548310, 4, 24, 0xb61a, "XXXvX.cpr" },
  { 1831050, 4, -1, 0x91d7, "end4.idx"  },
  {    102478, 5, 3, 0x886a, "OOOOvO.cpr" },
  {    926123, 5, 3, 0x671e, "OOOOvX.cpr" },
  {   1111011, 5, 3, 0xd4fc, "OOOvOO.cpr" },
  {   1838556, 5, 3, 0xa2ac, "OOOvXO.cpr" },
  {   1005367, 5, 3, 0x5ccd, "OOOvXX.cpr" },
  {    943757, 5, 3, 0x4010, "OOvOOO.cpr" },
  {   2300897, 5, 3, 0x68ad, "OOvXOO.cpr" },
  {   2102270, 5, 3, 0xe398, "OOvXXO.cpr" },
  {    715710, 5, 3, 0x54ee, "OOvXXX.cpr" },
  {    384439, 5, 3, 0x49ce, "OvOOOO.cpr" },
  {    291240, 5, 3, 0xdbc6, "OvXOOO.cpr" },
  {    271182, 5, 3, 0x3d7f, "OvXXOO.cpr" },
  {    140775, 5, 3, 0x1822, "OvXXXO.cpr" },
  {     31508, 5, 3, 0x0353, "OvXXXX.cpr" },
  {     96010, 5, 3, 0x2ee1, "XOOOvO.cpr" },
  {   1598227, 5, 3, 0xbc6b, "XOOOvX.cpr" },
  {   1623319, 5, 3, 0x4587, "XOOvOO.cpr" },
  {   4438023, 5, 3, 0x347c, "XOOvXO.cpr" },
  {   1218583, 5, 3, 0x1c4d, "XOOvXX.cpr" },
  {   1305895, 5, 3, 0x956f, "XOvOOO.cpr" },
  {   2087565, 5, 3, 0x8b9a, "XOvXOO.cpr" },
  {   2173717, 5, 3, 0x47f1, "XOvXXO.cpr" },
  {   1120920, 5, 3, 0x6d93, "XOvXXX.cpr" },
  {    560961, 5, 3, 0xd125, "XvOOOO.cpr" },
  {   2353517, 5, 3, 0xa154, "XvXOOO.cpr" },
  {   1474560, 5, 3, 0x07a0, "XvXXOO.cpr" },
  {    321272, 5, 3, 0xb9ed, "XvXXXO.cpr" },
  {     34366, 5, 3, 0x626a, "XvXXXX.cpr" },
  {    142178, 5, 3, 0x6634, "XXOOvO.cpr" },
  {   1159143, 5, 3, 0x32fc, "XXOOvX.cpr" },
  {   1252369, 5, 3, 0x13c6, "XXOvOO.cpr" },
  {   6779380, 5, 3, 0x77f4, "XXOvXO.cpr" },
  {   1629654, 5, 3, 0xaaa1, "XXOvXX.cpr" },
  {    655089, 5, 3, 0xbdf2, "XXvOOO.cpr" },
  {   1983928, 5, 3, 0x4ec6, "XXvXOO.cpr" },
  {    964488, 5, 3, 0xf331, "XXvXXO.cpr" },
  {    141643, 5, 3, 0x3365, "XXvXXX.cpr" },
  {    102616, 5, 3, 0xe25a, "XXXOvO.cpr" },
  {    420087, 5, 3, 0x74e3, "XXXOvX.cpr" },
  {    273072, 5, 3, 0x3387, "XXXvOO.cpr" },
  {   2707992, 5, 3, 0x4dee, "XXXvXO.cpr" },
  {    701054, 5, 3, 0xe919, "XXXvXX.cpr" },
  {     27937, 5, 3, 0xa500, "XXXXvO.cpr" },
  {     64147, 5, 3, 0xe207, "XXXXvX.cpr" },
  {   7234339, 6, 4, 0xd971, "OOOOvOO.cpr" },
  {  23494768, 6, 4, 0x3037, "OOOOvXO.cpr" },
  {  10320134, 6, 4, 0xf6ee, "OOOOvXX.cpr" },
  {   8772900, 6, 4, 0x7bab, "OOvOOOO.cpr" },
  {  20664945, 6, 4, 0xeb18, "OOvXOOO.cpr" },
  {  22817985, 6, 4, 0x6216, "OOvXXOO.cpr" },
  {  14314276, 6, 4, 0x9a5c, "OOvXXXO.cpr" },
  {   3598072, 6, 4, 0x6bc5, "OOvXXXX.cpr" },
  {   9548262, 6, 4, 0x7591, "XOOOvOO.cpr" },
  { 104771602, 6, 4, 0x6f77, "XOOOvXO.cpr" },
  {  21718073, 6, 4, 0x221a, "XOOOvXX.cpr" },
  {  14395823, 6, 4, 0xd92b, "XOvOOOO.cpr" },
  {  84891019, 6, 4, 0x6aed, "XOvXOOO.cpr" },
  { 160451697, 6, 4, 0xd0c2, "XOvXXOO.cpr" },
  { 112731973, 6, 4, 0xdc72, "XOvXXXO.cpr" },
  {  32416296, 6, 4, 0x8b1c, "XOvXXXX.cpr" },
  {   6480534, 6, 4, 0xa0f5, "XXOOvOO.cpr" },
  {  93929899, 6, 4, 0x67ac, "XXOOvXO.cpr" },
  {  48279904, 6, 4, 0x3da7, "XXOOvXX.cpr" },
  {   6855364, 6, 4, 0xca62, "XXvOOOO.cpr" },
  {  19138755, 6, 4, 0x0023, "XXvXOOO.cpr" },
  {  21129962, 6, 4, 0x4389, "XXvXXOO.cpr" },
  {  16008194, 6, 4, 0x49da, "XXvXXXO.cpr" },
  {   6468983, 6, 4, 0x8496, "XXvXXXX.cpr" },
  {   3506241, 6, 4, 0xbcac, "XXXOvOO.cpr" },
  {  40084503, 6, 4, 0x6f5b, "XXXOvXO.cpr" },
  {  43690342, 6, 4, 0x08ed, "XXXOvXX.cpr" },
  {    821062, 6, 4, 0x744e, "XXXXvOO.cpr" },
  {   6904967, 6, 4, 0x0146, "XXXXvXO.cpr" },
  {  12926925, 6, 4, 0xc5f8, "XXXXvXX.cpr" },
  {    244649, 6, 4, 0x2f4c, "OOOOOvO.cpr" },
  {   3350730, 6, 4, 0x9e0d, "OOOOOvX.cpr" },
  {    915053, 6, 4, 0x6eaa, "OvOOOOO.cpr" },
  {   1632688, 6, 4, 0x2380, "OvXOOOO.cpr" },
  {   2879263, 6, 4, 0xbdee, "OvXXOOO.cpr" },
  {   2954338, 6, 4, 0x09e4, "OvXXXOO.cpr" },
  {   1575066, 6, 4, 0xc01a, "OvXXXXO.cpr" },
  {    339753, 6, 4, 0x1a58, "OvXXXXX.cpr" },
  {   1179521, 6, 4, 0x05a2, "XOOOOvO.cpr" },
  {   4744837, 6, 4, 0xe5cc, "XOOOOvX.cpr" },
  {   2527260, 6, 4, 0x6763, "XvOOOOO.cpr" },
  {   5063251, 6, 4, 0x5f7e, "XvXOOOO.cpr" },
  {   5681974, 6, 4, 0x926b, "XvXXOOO.cpr" },
  {   4056861, 6, 4, 0xee75, "XvXXXOO.cpr" },
  {   1847962, 6, 4, 0x715f, "XvXXXXO.cpr" },
  {    384185, 6, 4, 0x9ce9, "XvXXXXX.cpr" },
  {   2581906, 6, 4, 0x23f8, "XXOOOvO.cpr" },
  {   6024692, 6, 4, 0x6e01, "XXOOOvX.cpr" },
  {   2821796, 6, 4, 0xa82c, "XXXOOvO.cpr" },
  {   4956211, 6, 4, 0x897c, "XXXOOvX.cpr" },
  {   1539164, 6, 4, 0x94fd, "XXXXOvO.cpr" },
  {   2315509, 6, 4, 0x1a5b, "XXXXOvX.cpr" },
  {    335200, 6, 4, 0xc7f1, "XXXXXvO.cpr" },
  {    460790, 6, 4, 0x7695, "XXXXXvX.cpr" },
  {  12675853, 6, 4, 0xd510, "OOOvOOO.cpr" },
  {  34899952, 6, 4, 0x29a0, "OOOvXOO.cpr" },
  {  35060337, 6, 4, 0xc88b, "OOOvXXO.cpr" },
  {  11722444, 6, 4, 0x7902, "OOOvXXX.cpr" },
  {  26592055, 6, 4, 0xd77a, "XOOvOOO.cpr" },
  {  70357335, 6, 4, 0x1cf5, "XOOvXOO.cpr" },
  {  47770514, 6, 4, 0x3638, "XOOvXXO.cpr" },
  {  14779414, 6, 4, 0x4457, "XOOvXXX.cpr" },
  {  21792974, 6, 4, 0x1c50, "XXOvOOO.cpr" },
  { 106575569, 6, 4, 0x0174, "XXOvXOO.cpr" },
  {  57675073, 6, 4, 0x318e, "XXOvXXO.cpr" },
  {   8600231, 6, 4, 0x8c4a, "XXOvXXX.cpr" },
  {   4489583, 6, 4, 0x5384, "XXXvOOO.cpr" },
  {  39089396, 6, 4, 0x1816, "XXXvXOO.cpr" },
  {  22468300, 6, 4, 0x8cb4, "XXXvXXO.cpr" },
  {   3300124, 6, 4, 0x97f5, "XXXvXXX.cpr" },
};

/* table for de-compression of 4-piece file contents (result value) */
char end_val[256] = {
  0,  1,  -2,  3,  -4,  5,  -6,  7,  -8,  9, -10, 11, -12,  13, -14,  15,
-16, 17, -18, 19, -20, 21, -22, 23, -24, 25, -26, 27, -28,  29, -30,  31,
-32, 33, -34, 35, -36, 37, -38, 39, -40, 41, -42, 43, -44,  45, -46,  47,
-48, 49, -50, 51, -52, 53, -54, 55, -56, 57, -58, 59, -60, 100, 100, 100,
  0,  1,  -2,  3,  -4,  5,  -6,  7,  -8,  9, -10, 11, -12,  13, -14,  15,
-16, 17, -18, 19, -20, 21, -22, 23, -24, 25, -26, 27, -28,  29, -30,  31,
-32, 33, -34, 35, -36, 37, -38, 39, -40, 41, -42, 43, -44,  45, -46,  47,
-48, 49, -50, 51, -52, 53, -54, 55, -56, 57, -58, 59, -60, 100, 100, 100,
  0,  1,  -2,  3,  -4,  5,  -6,  7,  -8,  9, -10, 11, -12,  13, -14,  15,
-16, 17, -18, 19, -20, 21, -22, 23, -24, 25, -26, 27, -28,  29, -30,  31,
-32, 33, -34, 35, -36, 37, -38, 39, -40, 41, -42, 43, -44,  45, -46,  47,
-48, 49, -50, 51, -52, 53, -54, 55, -56, 57, -58, 59, -60, 100, 100,   0,
  0,  1,  -2,  3,  -4,  5,  -6,  7,  -8,  9, -10, 11, -12,  13, -14,  15,
-16, 17, -18, 19, -20, 21, -22, 23, -24, 25, -26, 27, -28,  29, -30,  31,
-32, 33, -34, 35, -36, 37, -38, 39, -40, 41, -42, 43, -44,  45, -46,  47,
-48, 49, -50, 51, -52, 53, -54, 55, -56, 57, -58, 59, -60, 100, 100,   0,
};

/* table for de-compression of 4-piece file contents (repeat count) */
char end_amt[256] = {
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 5, 9,
2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 6, 10,
3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 7, 0,
4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 8, 0,
};

/* open end game database file if needed */
/* in: ep = ptr to endfile info structure */
/* returns: 0 is success     */
/*          1 file not found */
/*          2 incorrect size */
/*          3 other          */
static int open_endfile(endhf *ep)
{
    char *dbpath;

#ifdef _WIN32
    if (ep->hf == INVALID_HANDLE_VALUE)     /* got error opening file before */
    {
        return 3;
    }
    if (ep->hf == NULL)                     /* file not opened before */
    {
        dbpath = locate_dbfile(enddb_dirs, ep->name); /* get the full path */
        if (dbpath == NULL)
        {
            printf("open_endfile: %s not found\n", ep->name);
            return 1;
        }
        ep->hf = CreateFile(dbpath, GENERIC_READ, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
                        NULL);
        if (ep->hf == INVALID_HANDLE_VALUE)
        {
            printf("open_endfile: %s can't open\n", ep->name);
            return 1;
        }
        if (GetFileSize(ep->hf, NULL) != ep->size)
        {
            printf("open_endfile: %s wrong size\n", ep->name);
            CloseHandle(ep->hf);
            ep->hf = INVALID_HANDLE_VALUE;
            return 2;
        }
        ep->hmap = CreateFileMapping(ep->hf, NULL, PAGE_READONLY, 0, 0, ep->name);
        if (ep->hmap == NULL)
        {
            printf("open_endfile: %s CreateFileMapping failed\n", ep->name);
            CloseHandle(ep->hf);
            ep->hf = INVALID_HANDLE_VALUE;
            return 3;
        }
        ep->fptr = (u8 *) MapViewOfFile(ep->hmap, FILE_MAP_READ, 0, 0, 0);
        if (ep->fptr == NULL)
        {
            printf("open_endfile: %s MapViewOfFile failed\n", ep->name);
            CloseHandle(ep->hmap);
            ep->hmap = NULL;
            CloseHandle(ep->hf);
            ep->hf = INVALID_HANDLE_VALUE;
            return 3;
        }
    }
#else
    struct stat statbuf;
    int ret;

    if (ep->fd == -1)             /* got error opening file before */
    {
        return 3;
    }
    if (ep->fd == 0)              /* file not opened before */
    {
        dbpath = locate_dbfile(enddb_dirs, ep->name); /* get the full path */
        if (dbpath == NULL)
        {
            printf("open_endfile: %s not found\n", ep->name);
            ep->fd = -1;
            return 1;
        }
        ep->fd = open(dbpath, O_RDONLY, 0);
        if (ep->fd == -1)
        {
            printf("open_endfile: %s can't open\n", ep->name);
            return 1;
        }
        ret = fstat(ep->fd, &statbuf);
        if (ret != 0 || statbuf.st_size != ep->size)
        {
            close(ep->fd);
            ep->fd = -1;
            printf("open_endfile: %s wrong size\n", ep->name);
            return 2;
        }
        ep->fptr = (u8 *) mmap(NULL, ep->size, PROT_READ, MAP_SHARED, ep->fd, 0);
        if (ep->fptr == MAP_FAILED)
        {
            printf("open_endfile: %s mmap failed\n", ep->name);
            close(ep->fd);
            ep->fd = -1;
            return 3;
        }
        madvise(ep->fptr, ep->size, MADV_RANDOM);
    }
#endif
    return 0;
}

/* prepare for database indexing */
/* bb -> current board */
/* out: bitlist = positions of pieces  */
/*      epp = pptr to db info struct   */
/* returns: TRUE if successful,        */
/*          or FALSE if error/notfound */
static bool prep_db(bitboard *bb, u64 bitlist[], endhf **epp)
{
    u64 pcbits, pos, white, black, kings;

    if (bb->side == W)
    {
        /* use board contents as is */
        white = bb->white;
        black = bb->black;
        kings = bb->kings;
    }
    else
    {
        /* use inverted board contents */
        white = 0;
        pcbits = bb->black;
        while (pcbits != 0)
        {
            pos = pcbits & -pcbits;
            pcbits -= pos;
            white |= (1ULL << (53 - __builtin_ctzll(pos)));
        }
        black = 0;
        pcbits = bb->white;
        while (pcbits != 0)
        {
            pos = pcbits & -pcbits;
            pcbits -= pos;
            black |= (1ULL << (53 - __builtin_ctzll(pos)));
        }
        kings = 0;
        pcbits = bb->kings;
        while (pcbits != 0)
        {
            pos = pcbits & -pcbits;
            pcbits -= pos;
            kings |= (1ULL << (53 - __builtin_ctzll(pos)));
        }
    }
    /* eliminate ghost squares */
    white += (white & (G1 - 1));
    black += (black & (G1 - 1));
    kings += (kings & (G1 - 1));
    white += (white & (G2 - 1));
    black += (black & (G2 - 1));
    kings += (kings & (G2 - 1));
    white += (white & (G3 - 1));
    black += (black & (G3 - 1));
    kings += (kings & (G3 - 1));
    white += (white & (G4 - 1));
    black += (black & (G4 - 1));
    kings += (kings & (G4 - 1));
    white >>= 4;
    black >>= 4;
    kings >>= 4;
    /* fill list */
    bitlist[MW] = white & ~kings;
    bitlist[KW] = white & kings;
    bitlist[MB] = black & ~kings;
    bitlist[KB] = black & kings;
    /* find database file */
    *epp = end_ref[EF*EF*EF*popcount(bitlist[MW]) +
                   EF*EF*popcount(bitlist[KW]) +
                   EF*popcount(bitlist[MB]) +
                   popcount(bitlist[KB])];
    if (*epp == NULL)
    {
        return FALSE;
    }
    if (open_endfile(*epp) != 0)
    {
        end_acc[0]++;
        return FALSE;
    }
    return TRUE;
}

/* find exact value of current board in distance-to-win databases */
/* for upto 4 pieces */
/* bb -> current board */
/* ply = ply level */
/* out: valp = ptr to result value for side to move */
/* returns: TRUE if value found */
bool endgame_dtw(bitboard *bb, int ply, s32 *valp)
{
    endhf *ep;
    u8    c, *pb, *pz;
    u64   bitlist[4], pcbits, pos;
    u32   ipos, li, idx;
    int   i, ofs;

    if (enddb_dirs[0] == '\0')
    {
        return FALSE;               /* no endgame databases supplied */
    }
    switch (popcount(bb->white | bb->black))
    {
    case 2:
    case 3:
        if (!prep_db(bb, bitlist, &ep) || ep->fptr == NULL)
        {
            return FALSE;           /* specific egdb file not found/error */
        }
        ipos = 0;
        for (i = 0; i < 4; i++)
        {
            pcbits = bitlist[i];
            while (pcbits != 0)
            {
                pos = pcbits & -pcbits;
                pcbits -= pos;
                ipos = 50*ipos + __builtin_ctzll(pos);
            }
        }
        if (ipos >= (u32) ep->size)
        {
            end_acc[0]++;
            return FALSE;
        }
        c = ep->fptr[ipos];
        break;

    case 4:
#ifdef _WIN32
        if (end_ref[0]->hf == INVALID_HANDLE_VALUE)
#else
        if (end_ref[0]->fd == -1)
#endif
        {
            return FALSE;           /* 4-pc index file not available */
        }
        if (open_endfile(end_ref[0]) != 0 || end_ref[0]->fptr == NULL)
        {
            end_acc[0]++;           /* can't open 4-pc index file */
            return FALSE;
        }
        if (!prep_db(bb, bitlist, &ep) || ep->fptr == NULL)
        {
            return FALSE;           /* specific egdb file not found/error */
        }
        ipos = 0;
        for (i = 0; i < 4; i++)
        {
            pcbits = bitlist[i];
            while (pcbits != 0)
            {
                pos = pcbits & -pcbits;
                pcbits -= pos;
                ipos = 50*ipos + __builtin_ctzll(pos);
            }
        }
        li = ipos/256;
        ofs = (int) (ipos%256);
        idx = 0;
        if (li > 0)
        {
            pb = &end_ref[0]->fptr[ep->idx*73242 + li*3 - 3];
            if (pb > end_ref[0]->fptr + end_ref[0]->size - 3)
            {
                end_acc[0]++;       /* 4-pc index bounds check failure */
                return FALSE;
            }
            idx  = *pb++;           /* get 3-byte index, is little-endian */
            idx += *pb++*256;
            idx += *pb*65536;
        }
        pb = &ep->fptr[idx];        /* starting point for decompression */
        pz = ep->fptr + ep->size;   /* end of file marker */
        do {
            if (pb >= pz)
            {
                end_acc[0]++;       /* bounds check failure */
                return FALSE;
            }
            c = *pb++;
            if (c >= 255)           /* repeat code */
            {
                if (pb >= pz - 1)
                {
                    end_acc[0]++;
                    return FALSE;
                }
                ofs -= *pb++ + 1;   /* repeat count */
                c = end_val[*pb++];
            }
            else if (c == 191)      /* draw repeat code */
            {
                if (pb >= pz)
                {
                    end_acc[0]++;
                    return FALSE;
                }
                ofs -= *pb++ + 1;   /* draw repeat count */
                c = 100;
            }
            else
            {
                ofs -= end_amt[c];  /* repeat count */
                c = end_val[c];
            }
        } while (ofs >= 0);
        break;

    default:
        return FALSE;
    }

    /* determine dtw score, adjusting for root node level */
    i = (char) c;
    if (i == 100)
    {
        *valp = ep->matofs;         /* draw, return material value */
    }
    else if (i > 0)
    {
        *valp = INFIN - i - ply;    /* distance to win */
    }
    else
    {
        *valp = -INFIN - i + ply;   /* distance to loss */
    }
    end_acc[ep->pccount]++;
    return TRUE;
}

/* get index value for a single piece type */
/* in: sq = number of allowed board squares */
/*     pcbits = bitfield of piece positions */
/* returns: the index value                 */
__inline__
static u32 index_singletype(int sq, u64 pcbits)
{
    int n, leadingpc;
    u32 result;

    result = 0;
    while (pcbits != 0)
    {
        n = popcount(pcbits);
        leadingpc = __builtin_ctzll(pcbits);
        result += combi_array[sq][n]
                - combi_array[sq - leadingpc][n];
        sq -= leadingpc + 1;
        pcbits >>= leadingpc + 1;
    }
    return result;
}

/* find value of current board in win-draw-loss databases */
/* for 5 and 6 pieces, non-capture positions only */
/* bb -> current board */
/* out: valp = ptr to result value for side to move */
/* returns: TRUE if value found */
bool endgame_wdl(bitboard *bb, s32 *valp)
{
    endhf *ep;
    int   i;
    u32   ipos, p1, p2, p3;
    u8    cval, *blkptr, *pb, *pz;
    u64   bitlist[4];
    u64   mwbits, kwbits, mbbits, kbbits;
    u64   pcbits, pos;

    static u32 pow3[] = { 1, 3, 9, 27, 81 };

    if (enddb_dirs[0] == '\0')
    {
        return FALSE;                   /* no endgame databases supplied */
    }
    if (!prep_db(bb, bitlist, &ep) || ep->fptr == NULL)
    {
        return FALSE;                   /* specific egdb file not found/error */
    }

    mbbits = bitlist[MB];

    pcbits = bitlist[MB] & ~ROW1;
    mwbits = bitlist[MW];
    i = popcount(pcbits);
    while (pcbits != 0)                 /* remove white man index holes */
    {
        pos = pcbits & -pcbits;
        pcbits -= pos;
        mwbits += (mwbits & (pos - 1));
    }
    mwbits >>= 5 + i;

    pcbits = bitlist[MB] | bitlist[MW];
    kbbits = bitlist[KB];
    i = popcount(pcbits);
    while (pcbits != 0)                 /* remove black king index holes */
    {
        pos = pcbits & -pcbits;
        pcbits -= pos;
        kbbits += (kbbits & (pos - 1));
    }
    kbbits >>= i;

    pcbits = bitlist[MB] | bitlist[MW] | bitlist[KB];
    kwbits = bitlist[KW];
    i = popcount(pcbits);
    while (pcbits != 0)                 /* remove white king index holes */
    {
        pos = pcbits & -pcbits;
        pcbits -= pos;
        kwbits += (kwbits & (pos - 1));
    }
    kwbits >>= i;

    p3 = combi_array[50 - popcount(mbbits) - popcount(mwbits) -
                     popcount(kbbits)][popcount(kwbits)];
    p2 = p3*combi_array[50 - popcount(mbbits) - popcount(mwbits)]
                       [popcount(kbbits)];
    p1 = p2*combi_array[45][popcount(mwbits)];
    ipos = index_singletype(45, mbbits)*p1
         + index_singletype(45, mwbits)*p2
         + index_singletype(50 - popcount(mbbits) - popcount(mwbits),
                            kbbits)*p3
         + index_singletype(50 - popcount(mbbits) - popcount(mwbits) -
                            popcount(kbbits), kwbits);

    blkptr = ep->fptr + ep->idx*(ipos/1024);
    pz = ep->fptr + ep->size;           /* end of file marker */
    if (blkptr > pz - ep->idx)
    {
        end_acc[0]++;
        return FALSE;
    }
    pb = ep->fptr + *blkptr++;          /* find start of segment */
    pb += *blkptr++*256;                /* note: is little-endian */
    pb += *blkptr++*65536;
    if (ep->idx > 3)
    {
        pb += *blkptr*16777216;         /* for 4-byte index */
    }
    i = (int) (ipos%1024);
    do {
        if (pb >= pz)                   /* bounds check */
        {
            end_acc[0]++;
            return FALSE;
        }
        cval = *pb++;
        if (cval <= 242)                /* non-repeat code */
        {
            i -= 5;
        }
        else if (cval <= 246)           /* repeat code for win */
        {
            if (cval == 246)
            {
                if (pb >= pz)
                {
                    end_acc[0]++;
                    return FALSE;
                }
                i -= *pb++*5;           /* next byte has repeat count */
            }
            else
            {
                 i -= (cval - 241)*5;   /* repeat count 2,3,4 */
            }
            cval = 0;
        }
        else if (cval <= 250)           /* repeat code for draw */
        {
            if (cval == 250)
            {
                if (pb >= pz)
                {
                    end_acc[0]++;
                    return FALSE;
                }
                i -= *pb++*5;           /* next byte has repeat count */
            }
            else
            {
                i -= (cval - 245)*5;    /* repeat count 2,3,4 */
            }
            cval = 121;
        }
        else if (cval <= 254)           /* repeat code for loss */
        {
            if (cval == 254)
            {
                if (pb >= pz)
                {
                    end_acc[0]++;
                    return FALSE;
                }
                i -= *pb++*5;           /* next byte has repeat count */
            }
            else
            {
                i -= (cval - 249)*5;    /* repeat count 2,3,4 */
            }
            cval = 242;
        }
        else                            /* 255 = repeat code for other */
        {
            if (pb >= pz - 1)
            {
                end_acc[0]++;
                return FALSE;
            }
            i -= *pb++*5;               /* next byte has repeat count */
            cval = *pb++;               /* next byte has repeated value */
        }
    } while (i >= 0);
    cval = (cval/pow3[4 + (i + 1)%5])%3;
    if (cval == 1)                      /* draw */
    {
        *valp = ep->matofs;             /* add small material offset */
    }
    else                                /* win or loss */
    {
        *valp = INFIN - (MAXEXACT + MAX5PLY)/2;
        if (ep->pccount == 6)           /* value depends on total */
        {                               /* number of pieces       */
            *valp = INFIN - (MAX5PLY + MAXPLY)/2;
        }
        if (cval == 2)
        {
            *valp = -*valp;             /* loss */
        }
        /* adjust for material and positional differences, */
        /* encouraging promotions and conversions */
        *valp += 10*popcount(bitlist[KW]) - 10*popcount(bitlist[KB]);
        pcbits = bitlist[MW];
        while (pcbits != 0)
        {
            pos = pcbits & -pcbits;
            pcbits -= pos;
            *valp += (49 - __builtin_ctzll(pos))/5;
        }
        pcbits = bitlist[MB];
        while (pcbits != 0)
        {
            pos = pcbits & -pcbits;
            pcbits -= pos;
            *valp -= __builtin_ctzll(pos)/5;
        }
    }
    end_acc[ep->pccount]++;
    return TRUE;
}

/* find endgame value of current board */
/* bb -> current board */
/* ply = ply level */
/* out: valp = ptr to result value for side to move */
/* returns: TRUE if value found */
bool endgame_value(bitboard *bb, int ply, s32 *valp)
{
    movelist list;
    int  pcnt, m;
    s32  best, score;

    /* check DTW endgame database */
    pcnt = popcount(bb->white | bb->black);
    if (pcnt <= DTWENDPC && endgame_dtw(bb, ply, valp))
    {
        return TRUE;
    }

    /* generate all moves */
    gen_moves(bb, &list, NULL, TRUE);
    if (list.count == 0)
    {
        /* side to move can't move */
        *valp = -INFIN + ply;
        return TRUE;
    }

    /* check WDL endgame database */
    if (pcnt > DTWENDPC && pcnt <= MAXENDPC)
    {
        if (list.npcapt == 0) /* non-capture position */
        {
            if (endgame_wdl(bb, valp))
            {
                return TRUE;
            }
        }
        else /* play out the captures until quiescence reached */
        {
            best = -INFIN;
            for (m = 0; m < list.count; m++)
            {
                if (!endgame_value(&list.move[m], ply + 1, &score))/* recurse */
                {
                    return FALSE;
                }
                score = -score; /* convert for other side to move */
                if (score > best)
                {
                    best = score;
                }
            }
            *valp = best;
            return TRUE;
        }
    }
    return FALSE;
}

/* check presence & correct contents of end game files */
/* (takes a long time) */
void check_enddb(void)
{
    endhf *ep;
    u16 crc, x;
    int i, n, ret, total, correct;

    total = correct = 0;
    for (i = 0; i < elements(end_ref); i++)
    {
        ep = end_ref[i];
        if (ep != NULL)
        {
            total++;
            ret = open_endfile(ep);
            if (ret == 0)
            {
                if (ep->fptr == NULL)
                {
                    printf("check_enddb: %s not mmap'ed\n", ep->name);
                    continue; /* with next file */
                }
                /* file integrity check using CRC16 algorithm, variant: */
                /* width=16 poly=0x1021 init=0xffff refin=false refout=false */
                /* xorout=0x0000 check=0x29b1 name="CRC-16/CCITT-FALSE" */
                crc = 0xffff;
                for (n = 0; n < ep->size; n++)
                {
                    x = (crc >> 8) ^ ep->fptr[n];
                    x ^= (x >> 4);
                    crc = (crc << 8) ^ (x << 12) ^ (x << 5) ^ x;
                }
                if (crc != ep->crc)
                {
                    printf("check_enddb: %s wrong crc %04x, expected %04x\n",
                           ep->name, crc, ep->crc);
                    continue; /* with next file */
                }
                printf("%s OK\n", ep->name);
                correct++;
            }
        }
        fflush(stdout);
    }
    fprintf(stderr, "%d out of %d db files present and correct\n",
            correct, total);
}

/* initialize the endgame databases for use */
void init_enddb(char *dirs)
{
    endhf *ep;
    int i, j;
    int mw, kw, mb, kb;
    int present[MAXENDPC + 1];
    int total[MAXENDPC + 1];

    strncpy(enddb_dirs, dirs, sizeof enddb_dirs - 1);
    memset(present, 0, sizeof present);
    memset(total, 0, sizeof total);
    for (i = 0; i < elements(end_set); i++)
    {
        ep = &end_set[i];
        j = mw = kw = mb = kb = 0;
        while (ep->name[j] != 'v' && ep->name[j] != '\0')
        {
            if (ep->name[j] == 'O')
            {
                mw++;
            }
            if (ep->name[j] == 'X')
            {
                kw++;
            }
            j++;
        }
        while (ep->name[j] != '.' && ep->name[j] != '\0')
        {
            if (ep->name[j] == 'O')
            {
                mb++;
            }
            if (ep->name[j] == 'X')
            {
                kb++;
            }
            j++;
        }
        ep->matofs = mw + 2*kw - mb - 2*kb;
        end_ref[EF*EF*EF*mw + EF*EF*kw + EF*mb + kb] = ep;
        total[ep->pccount]++;
        if (locate_dbfile(enddb_dirs, ep->name) != NULL)
        {
            present[ep->pccount]++;
        }
    }
    for (i = 2; i <= MAXENDPC; i++)
    {
        if (present[i] == total[i])
        {
            printf("all %d-piece db files present\n", i);
        }
        else
        {
            printf("init_enddb: %d out of %d %d-piece db files not found\n",
                   total[i] - present[i], total[i], i);
        }
    }

    combi_array[0][0] = 1;        /* set up combination lookup table */
    for (i = 1; i <= 50; i++)
    {
        combi_array[i][0] = 1;
        for (j = 1; j < 8; j++)
        {
            combi_array[i][j] = combi_array[i - 1][j - 1] + combi_array[i - 1][j];
        }
    }
}
