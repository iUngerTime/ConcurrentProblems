#*************************************
# Makefile for concurrent binary tree
# 
# Author: Brenton Unger
# Date: Jan 18, 2020
#
CPPFLAGS = -g -o3 -Wall -std=c++11 -pthread

OBJS = cbinary.o \
       usec.o

all: main

clean:
	rm -f main
	rm -f *.o

.c.o:
	g++ $(CPPFLAGS) -c $? -o $@

main: main.cpp $(OBJS)
	g++ $(CPPFLAGS) main.cpp -o main $(OBJS)
