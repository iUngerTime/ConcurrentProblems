EXECS = mpisort
CC = mpicc
CFLAGS = -O3 -Wall -g -std=c99

OBJS = usec.o

all: ${EXECS}

mpisort: mpisort.c $(OBJS)
	$(CC) $(CFLAGS) -o mpisort mpisort.c $(OBJS)

clean:
	rm -rf $(EXECS)
	rm -rf $(OBJS)
	rm -rf out
