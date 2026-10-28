#!/usr/bin/env bash
# Times the static and dynamic schedules side by side and prints Markdown tables.
#
# Usage: ./bench.sh [thread counts...]    e.g. ./bench.sh 1 2 4 8 16
# Defaults to 1, 2, 4, ... up to the number of CPUs. REPEATS sets best-of-N (default 3).
set -euo pipefail
cd "$(dirname "$0")"

CC=${CC:-gcc}
REPEATS=${REPEATS:-3}
CPUS=$(getconf _NPROCESSORS_ONLN)

# lodepng is vendored third-party code, so its warnings are silenced
$CC -O3 -fopenmp -w -c -o lodepng.o lib/lodepng.c
$CC -O3 -fopenmp -Wall -Wextra -DUSE_STATIC_SCHEDULE -o mandelbrot_static mandelbrot.c lodepng.o -lm
$CC -O3 -fopenmp -Wall -Wextra -o mandelbrot_dynamic mandelbrot.c lodepng.o -lm

if [ $# -gt 0 ]; then
  threads=("$@")
else
  threads=()
  t=1
  while [ "$t" -le "$CPUS" ]; do threads+=("$t"); t=$((t * 2)); done
  if [ "${threads[${#threads[@]}-1]}" -ne "$CPUS" ]; then threads+=("$CPUS"); fi
fi

# Fastest of $REPEATS runs, in seconds: best_time BINARY THREADS
best_time() {
  local best="" run
  for _ in $(seq "$REPEATS"); do
    run=$(OMP_NUM_THREADS=$2 "./$1" | sed -nE 's/.*took ([0-9.]+) seconds.*/\1/p')
    best=$(awk -v a="$run" -v b="${best:-$run}" 'BEGIN { if (a < b) print a; else print b }')
  done
  echo "$best"
}

echo "Best of $REPEATS runs on $CPUS CPUs ($(uname -sm)), speedup vs. the first thread count:"
echo
echo "| Threads | Static (s) | Speedup | Dynamic (s) | Speedup |"
echo "|---:|---:|---:|---:|---:|"
base_static=""
base_dynamic=""
for t in "${threads[@]}"; do
  s=$(best_time mandelbrot_static "$t"); mv out.png out_static.png
  d=$(best_time mandelbrot_dynamic "$t"); mv out.png out_dynamic.png
  base_static=${base_static:-$s}
  base_dynamic=${base_dynamic:-$d}
  awk -v t="$t" -v s="$s" -v d="$d" -v bs="$base_static" -v bd="$base_dynamic" \
    'BEGIN { printf "| %d | %.2f | %.2fx | %.2f | %.2fx |\n", t, s, bs / s, d, bd / d }'
done

echo
if cmp -s out_static.png out_dynamic.png; then
  echo "Both schedules produced byte-identical images."
else
  echo "WARNING: the two schedules produced different images." >&2
fi

# The work per row is the same on every machine, so one run is enough
OMP_NUM_THREADS=$CPUS ./mandelbrot_dynamic --profile | sed -n '/^$/,$p'
