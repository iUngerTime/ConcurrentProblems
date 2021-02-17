# -O0 is good for debugging, but might want to try -O3 for performance reasons
CPPFLAGS = -g -O3 -Wall -std=c++11

all: mandelbrot

mandelbrot:
	mpic++ mpi_mandelbrot.cpp $(CPPFLAGS) -o mpibrot

clean:
	rm -f mpibrot
	rm -f *.bmp
