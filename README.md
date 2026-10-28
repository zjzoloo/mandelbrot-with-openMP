# Parallel Mandelbrot in C with OpenMP
 
Renders a 1920×1080 image of the Mandelbrot set (up to 8,000 iterations per pixel) on all cores with OpenMP, and shows why the loop schedule decides how well it scales.
 
![The Mandelbrot set as rendered by this program](docs/mandelbrot.png)
 
## Background
 
I used this code as the benchmark in my seminar paper, *Comparison of OpenMP SIMD to Auto-Vectorization*, at RWTH Aachen's Chair for High Performance Computing, with runs on the university's CLAIX cluster (Intel Xeon Platinum 8160, GCC 4.8.5 and ICC 19.0.1). Two results in the paper had no explanation yet: 3 threads ran slower than 2, and the best speedup was only 19.2× with GCC (15.1× with ICC) at 48 threads. This version finds the cause and fixes it.
 
## The cause: unequal work per row
 
Pixels inside the set (orange in the image above) run all 8,000 iterations, while most pixels outside it (blue) escape after a few. A row therefore costs as much as the part of the set it crosses, and the rows through the middle of the image are by far the most expensive.
 
The original loop used OpenMP's default static schedule, which gives every thread one contiguous block of rows of equal size. Counting the iterations per row (`./mandelbrot --profile`) shows how unevenly that splits the work:
 
| Threads | Busiest thread's share of the work | Static speedup limit |
|---:|---:|---:|
| 2 | 50.2% | 1.99x |
| 3 | 68.1% | 1.47x |
| 4 | 45.1% | 2.22x |
| 6 | 34.2% | 2.93x |
| 8 | 27.6% | 3.62x |
| 12 | 19.6% | 5.10x |
| 16 | 15.1% | 6.62x |
| 24 | 10.1% | 9.86x |
| 32 | 7.8% | 12.80x |
| 40 | 6.2% | 16.03x |
| 48 | 5.4% | 18.67x |
 
- **3 threads:** the middle thread gets 68% of the work, so 3 threads can be at most 1.47× faster than 1, which is slower than 2 threads (1.99×). That is the dip measured in the paper.
- **48 threads:** at most 18.7×, essentially the 19.2× measured on CLAIX. The original loop split the image by pixels rather than whole rows, so the two numbers differ slightly.
These limits come from counting iterations, so they hold on any machine.
 
## The fix: dynamic scheduling
 
`schedule(dynamic)` hands out one row at a time, so whichever thread is free takes the next row. The largest single row is only 0.31% of the work, so the schedule no longer caps the speedup.
 
Best of 3 runs on a 2-vCPU cloud VM (Intel Xeon, GCC 13):
 
| Threads | Static (s) | Speedup | Dynamic (s) | Speedup |
|---:|---:|---:|---:|---:|
| 1 | 6.15 | 1.00x | 6.28 | 1.00x |
| 2 | 3.15 | 1.96x | 3.10 | 2.02x |
 
With 2 threads both schedules perform the same: the image is symmetric about the real axis, so each half carries almost exactly half the work. The difference starts at 3 threads; `make bench` measures it on a machine with more cores. Both schedules produce byte-identical images.
 
## Other changes from the first version
 
- The 8.3 MB image buffer moved from the stack to the heap. On the stack it sat about 90 KB below the common 8 MiB default limit, so any larger resolution would have crashed.
- Building with `-DUSE_STATIC_SCHEDULE` restores the old schedule for comparison.
- Loop variables are declared inside the loops instead of listed in a `private` clause, and PNG write errors are reported.
The rendered image is byte-identical to the first version's.
 
## Build and run
 
Needs GCC or another compiler with OpenMP support (on macOS, Apple's clang additionally needs libomp).
 
```sh
make            # optimized build
make run        # render and open out.png (uses eog; see the Makefile for macOS)
make profile    # work per row and the static-schedule limits
make bench      # static vs. dynamic timings, best of 3
```
 
`bash bench.sh 1 2 4 8` runs custom thread counts. `mandelbrot.slurm` runs a scaling test as a batch job on CLAIX.
 
## Credits
 
The rendering code comes from the [PPCES 2021](https://blog.rwth-aachen.de/itc-events/event/ppces-2021/) OpenMP workshop by RWTH Aachen's IT Center. PNG output uses [LodePNG](https://lodev.org/lodepng/) by Lode Vandevenne (zlib license), vendored in `lib/`.
 
