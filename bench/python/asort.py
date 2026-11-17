# bench/asort.prg in Python: the library's sort, 300,000 numbers ascending, then 100,000 with a comparison function
# (the .prg's block), 5 times.
from functools import cmp_to_key

from bench import report, start

DESCENDING = cmp_to_key(lambda p, q: (p < q) - (p > q))


def sort_both():
    x, a, b = 777, [0] * 300000, [0] * 100000
    for i in range(len(a)):
        x = (x * 16807) % 2147483647
        a[i] = x % 1000000
    x = 4242
    for i in range(len(b)):
        x = (x * 16807) % 2147483647
        b[i] = x % 1000000
    a.sort()
    b.sort(key=DESCENDING)
    total = sum(a[0::1000])
    return (a[0] == 8 and a[149999] == 500673 and a[299999] == 999998 and total == 149589926 and b[0] == 999994
            and b[49999] == 501282 and b[99999] == 8)


t = start()
report("asort", t, all([sort_both() for _ in range(5)]))
