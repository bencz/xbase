# bench/array_preallocate.prg in Python: lists made at their size, 25 lists of 2,000,000 numbers made with [None] * n,
# filled by index, then summed by index (array_append builds the same lists with append()).
from bench import report, start


def run():
    n, total, a = 2000000, 0, None
    for r in range(1, 26):
        a = [None] * n
        for i in range(n):
            a[i] = i + 1 + r
        for i in range(n):
            total += a[i]
    return total == 50000675000000 and len(a) == n


t = start()
report("array_preallocate", t, run())
