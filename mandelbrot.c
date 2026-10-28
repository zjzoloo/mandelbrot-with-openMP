/*
 * Parallel Mandelbrot renderer in C with OpenMP.
 *
 * Renders a WIDTH x HEIGHT image of the Mandelbrot set to out.png. Pixels inside
 * the set run all MAX_ITER iterations, while most pixels outside escape after a
 * few, so the cost of a row varies enormously. Rows are therefore handed out
 * dynamically: a thread that finishes a row takes the next free one.
 *
 * Build with -DUSE_STATIC_SCHEDULE for the static-schedule baseline.
 * Run with --profile to see how unevenly a static schedule splits the work.
 */
#include "lib/lodepng.h"
#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 1920
#define HEIGHT 1080
#define MAX_ITER 8000
#define X_MIN -2.5
#define X_MAX 1.5
#define PI 3.1415927
#define OUTPUT "out.png"

#ifdef USE_STATIC_SCHEDULE
#define SCHEDULE_NAME "static"
#else
#define SCHEDULE_NAME "dynamic"
#endif

/* Iterations spent on each row: the actual work, independent of the machine. */
static long long row_work[HEIGHT];

/*
 * schedule(static) without a chunk size gives each thread one contiguous block
 * of rows of (almost) equal size, so the busiest block bounds the speedup.
 * schedule(dynamic) hands out single rows, so only the largest row bounds it.
 */
static void report_load_balance(void) {
    const int thread_counts[] = {2, 3, 4, 6, 8, 12, 16, 24, 32, 40, 48};
    long long total = 0, largest_row = 0;
    for (int y = 0; y < HEIGHT; y++) {
        total += row_work[y];
        if (row_work[y] > largest_row) largest_row = row_work[y];
    }

    printf("\n| Threads | Busiest thread's share of the work | Static speedup limit |\n");
    printf("|---:|---:|---:|\n");
    for (size_t k = 0; k < sizeof thread_counts / sizeof thread_counts[0]; k++) {
        const int threads = thread_counts[k];
        const int rows_each = HEIGHT / threads, extra = HEIGHT % threads;
        long long busiest = 0;
        int row = 0;
        for (int t = 0; t < threads; t++) {
            const int rows = rows_each + (t < extra ? 1 : 0);
            long long work = 0;
            for (int r = 0; r < rows; r++) work += row_work[row++];
            if (work > busiest) busiest = work;
        }
        printf("| %d | %.1f%% | %.2fx |\n", threads, 100.0 * busiest / total,
               (double)total / busiest);
    }
    printf("\nWith schedule(dynamic), the largest single row is %.2f%% of the work.\n",
           100.0 * largest_row / total);
}

int main(int argc, char **argv) {
    const int profile = argc > 1 && strcmp(argv[1], "--profile") == 0;

    /* 8.3 MB: just under the common 8 MiB default stack limit, so it lives on the heap. */
    unsigned char *image = malloc((size_t)WIDTH * HEIGHT * 4);
    if (image == NULL) {
        fprintf(stderr, "Could not allocate the image buffer.\n");
        return EXIT_FAILURE;
    }

    const double half_height = (HEIGHT / (double)WIDTH) * (X_MAX - X_MIN) / 2;
    const double y_top = half_height;
    const double pw = (X_MAX - X_MIN) / WIDTH;
    const double ph = (-half_height - half_height) / HEIGHT; /* rows run top to bottom */

    const double start = omp_get_wtime();

#ifdef USE_STATIC_SCHEDULE
    #pragma omp parallel for schedule(static)
#else
    #pragma omp parallel for schedule(dynamic)
#endif
    for (int y = 0; y < HEIGHT; y++) {
        const double ry = y_top + y * ph;
        long long iterations = 0;
        for (int x = 0; x < WIDTH; x++) {
            const double rx = X_MIN + x * pw;
            double zx = 0.0, zy = 0.0, zx2 = 0.0, zy2 = 0.0;
            int i;
            for (i = 0; i < MAX_ITER && (zx2 + zy2) < 4; i++) {
                zy = 2 * zx * zy + ry;
                zx = zx2 - zy2 + rx;
                zx2 = zx * zx;
                zy2 = zy * zy;
            }
            iterations += i;
            unsigned char *px = image + ((size_t)y * WIDTH + x) * 4;
            px[0] = 255 - (cos(i * PI / (double)MAX_ITER) + 1) / 2 * 255;
            px[1] = 255 - (sin(i * PI / (double)MAX_ITER) + 1) / 3 * 255;
            px[2] = 255 - (i / (double)MAX_ITER) * 255;
            px[3] = 255;
        }
        row_work[y] = iterations; /* each row belongs to exactly one thread */
    }

    printf("Calculation took %lf seconds using %d threads (%s schedule).\n",
           omp_get_wtime() - start, omp_get_max_threads(), SCHEDULE_NAME);

    if (profile) report_load_balance();

    const unsigned error = lodepng_encode32_file(OUTPUT, image, WIDTH, HEIGHT);
    free(image);
    if (error) {
        fprintf(stderr, "Could not write %s: %s\n", OUTPUT, lodepng_error_text(error));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
