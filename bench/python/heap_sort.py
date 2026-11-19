# bench/heap_sort.prg in Python: heap sort in place (a binary max-heap built bottom-up, then sifted down) of 300,000
# integers from a linear congruential generator, 3 times with different seeds; the order and a weighted checksum are
# checked. The list keeps the .prg's 1-based indexes (element 0 unused).
from bench import report, start


def sift_down(a, i, n):
    x = a[i]
    while (c := i * 2) <= n:
        if c < n and a[c + 1] > a[c]:
            c += 1
        if a[c] <= x:
            break
        a[i] = a[c]
        i = c
    a[i] = x


def run():
    n, ok, total = 300000, True, 0
    a = [0] * (n + 1)
    for rep in range(1, 4):
        seed = rep + 10
        for i in range(1, n + 1):
            seed = seed * 16807 % 2147483647
            a[i] = seed
        for i in range(n // 2, 0, -1):
            sift_down(a, i, n)
        for i in range(n, 1, -1):
            a[1], a[i] = a[i], a[1]
            sift_down(a, 1, i - 1)
        for i in range(2, n + 1):
            if a[i - 1] > a[i]:
                ok = False
        for i in range(1, n + 1):
            total += a[i] * (i % 8 + 1)
    return ok and total == 4351668891394344


t = start()
report("heap_sort", t, run())
