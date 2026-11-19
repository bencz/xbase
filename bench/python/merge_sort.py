# bench/merge_sort.prg in Python: recursive top-down merge sort of 300,000 integers from a linear congruential
# generator, through one work list, 3 times with different seeds; the order and a weighted checksum of the sorted
# values are checked.
from bench import report, start


def merge_sort(a, w, lo, hi):
    if hi - lo < 1:
        return
    mid = (lo + hi) // 2
    merge_sort(a, w, lo, mid)
    merge_sort(a, w, mid + 1, hi)
    i, j, k = lo, mid + 1, lo
    while i <= mid and j <= hi:
        if a[j] < a[i]:
            w[k] = a[j]
            j += 1
        else:
            w[k] = a[i]
            i += 1
        k += 1
    while i <= mid:
        w[k] = a[i]
        i += 1
        k += 1
    while j <= hi:
        w[k] = a[j]
        j += 1
        k += 1
    for k in range(lo, hi + 1):
        a[k] = w[k]


def run():
    n, ok, total = 300000, True, 0
    a, w = [0] * n, [0] * n
    for rep in range(1, 4):
        seed = rep
        for i in range(n):
            seed = seed * 16807 % 2147483647
            a[i] = seed
        merge_sort(a, w, 0, n - 1)
        for i in range(1, n):
            if a[i - 1] > a[i]:
                ok = False
        for i in range(n):
            total += a[i] * ((i + 1) % 8 + 1)
    return ok and total == 4349496350073402


t = start()
report("merge_sort", t, run())
