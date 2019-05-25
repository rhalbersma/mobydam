CC=gcc
CFLAGS=-g -O3 -Wall -mpopcnt -flto $(PROF) -DPF -DETC -DLMR -DKIL -DCUT
# -D_DEBUG
#CFLAGS=-g -O2 -Wall -mpopcnt -flto $(PROF) -DPF -DETC -DLMR -DKIL -DCUT
#CFLAGS=-g -Wall -mpopcnt -fno-inline -D_DEBUG -DPF -DETC -DLMR -DKIL -DCUT
#CFLAGS=-g -pg -O2 -Wall -mpopcnt -fno-inline -DPF -DETC -DLMR -DKIL -DCUT
#CFLAGS=-g -Wall -mpopcnt -fprofile-arcs -ftest-coverage

VPATH = .:../core

SRCS = dxp.c pdn.c search.c move.c book.c break.c end.c eval.c tt.c util.c 
OBJS = dxp.o pdn.o search.o move.o book.o break.o end.o eval.o tt.o util.o 
HDRS = dxp.h pdn.h search.h move.h book.h break.h end.h eval.h tt.h util.h core.h main.h Makefile

lin: mobydam
win: mobydam.exe

$(OBJS): $(HDRS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

dxp.o: $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) -DCFLAGS="$(CFLAGS)" -c $<

mobydam: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $+

mobydam.exe: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $+ -lws2_32 -lwinmm

clean:
	rm -f mobydam mobydam.exe *.o *.gcda *.gcno gmon.out

uno: $(SRCS)
	uno -D_DEBUG -DPF -DETC -DLMR -DKIL -DCUT $+
