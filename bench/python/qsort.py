# bench/qsort.prg in Python: recursive quicksort over 200,000 Park-Miller numbers, 5 times.
from bench import report, start


def qs(a, lo, hi):
    while lo < hi:
        pivot = a[(lo + hi) // 2]
        i, j = lo, hi
        while i <= j:
            while a[i] < pivot:
                i += 1
            while a[j] > pivot:
                j -= 1
            if i <= j:
                a[i], a[j] = a[j], a[i]
                i += 1
                j -= 1
        if j - lo < hi - i:
            qs(a, lo, j)
            lo = i
        else:
            qs(a, i, hi)
            hi = j


def sort_random():
    n, x = 200000, 12345
    a = [0] * (n + 1)  # 1-based, as the xBase array
    for i in range(1, n + 1):
        x = (x * 16807) % 2147483647
        a[i] = x % 1000000
    qs(a, 1, n)
    is_sorted = all(a[i - 1] <= a[i] for i in range(2, n + 1))
    total = sum(a[i] for i in range(1, n + 1, 1000))
    return is_sorted and a[1] == 6 and a[100000] == 500294 and a[n] == 999998 and total == 99556483


t = start()
report("qsort", t, all([sort_random() for _ in range(5)]))
