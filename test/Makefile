CC = gcc
CFLAGS=-g -O2 -Wall -march=native -flto
#CFLAGS=-g -Wall -march=native -D_DEBUG
#CFLAGS=-g -pg -O2 -Wall -march=native -fno-inline -D_DEBUG
#CFLAGS=-g -pg -Wall -march=native -fno-inline -D_DEBUG
#CFLAGS=-g -Wall -march=native -fprofile-arcs -ftest-coverage -D_DEBUG

VPATH = .:../core

# core files:
SRCS = book.c break.c end.c eval.c move.c tt.c util.c 
OBJS = book.o break.o end.o eval.o move.o tt.o util.o 
HDRS = book.h break.h end.h eval.h move.h tt.h util.h core.h test.h Makefile

lin: movegen perft perftval val sizes fen2dxp endver mm bookgen
win: movegen.exe perft.exe perftval.exe val.exe sizes.exe fen2dxp.exe endver.exe mm.exe bookgen.exe

$(OBJS): $(HDRS)
gen.o perft.o perftval.o val.o sizes.o fen2dxp.o endver.o mm.o bookgen.o: $(HDRS)

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

movegen movegen.exe: movegen.o move.o util.o
	$(CC) $(CFLAGS) -o $@ $+

perft perft.exe: perft.o move.o util.o
	$(CC) $(CFLAGS) -o $@ $+

perftval perftval.exe: perftval.o break.o eval.o move.o util.o
	$(CC) $(CFLAGS) -o $@ $+

val val.exe: val.o break.o end.o eval.o move.o util.o
	$(CC) $(CFLAGS) -o $@ $+

sizes sizes.exe: sizes.o
	$(CC) $(CFLAGS) -o $@ $+

fen2dxp fen2dxp.exe: fen2dxp.o move.o util.o
	$(CC) $(CFLAGS) -o $@ $+

endver endver.exe: endver.o end.o move.o util.o
	$(CC) $(CFLAGS) -o $@ $+

mm: mm.o util.o
	$(CC) $(CFLAGS) -o $@ $+

mm.exe: mm.o util.o
	$(CC) $(CFLAGS) -o $@ $+ -lws2_32 -lwinmm

bookgen bookgen.exe: bookgen.o book.o move.o util.o
	$(CC) $(CFLAGS) -o $@ $+

clean:
	rm -f movegen perft perftval val sizes fen2dxp endver mm bookgen \
    *.o *.exe *.gcda *.gcno gmon.out

uno: $(SRCS)
	uno -D_DEBUG movegen.c $+
	uno -D_DEBUG perft.c $+
	uno -D_DEBUG perftval.c $+
	uno -D_DEBUG val.c $+
	uno -D_DEBUG sizes.c
	uno -D_DEBUG fen2dxp.c $+
	uno -D_DEBUG endver.c $+
	uno -D_DEBUG mm.c $+
	uno -D_DEBUG bookgen.c $+
