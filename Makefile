.PHONY: clean run go debug release run_batch profile bench

OBJECTS=lodepng.o mandelbrot.o
CC ?= gcc
FLAGS_OPENMP ?= -fopenmp
LINKERFLAGS=-lm

release: CFLAGS=-O3 $(FLAGS_OPENMP)
release: clean $(OBJECTS)
	$(CC) $(CFLAGS) -Wall -o mandelbrot $(OBJECTS) $(LINKERFLAGS)

debug: CFLAGS=-g -O0 $(FLAGS_OPENMP)
debug: clean $(OBJECTS)
	$(CC) $(CFLAGS) -Wall -o mandelbrot $(OBJECTS) $(LINKERFLAGS)

mandelbrot.o: mandelbrot.c
	$(CC) $(CFLAGS) -Wall -Wextra -c -o $@ $<

# lodepng is vendored third-party code, so its warnings are silenced
lodepng.o: lib/lodepng.c
	$(CC) $(CFLAGS) -w -c -o $@ $<

run go:
	./mandelbrot && eog out.png
	# on macos: comment out above and use the following instead:
	# ./mandelbrot && open out.png

run_batch:
	./mandelbrot

profile:
	./mandelbrot --profile

bench:
	bash bench.sh

clean:
	rm -f *.o mandelbrot mandelbrot_static mandelbrot_dynamic out*.png
