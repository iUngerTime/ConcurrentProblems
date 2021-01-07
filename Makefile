# -O0 is good for debugging, but might want to try -O3 for performance reasons
CPPFLAGS = -g -O3 -Wall -std=c++11 -pthread

all: mandelbrot

clean:
	rm -f mandelbrot
	rm -f *.bmp
