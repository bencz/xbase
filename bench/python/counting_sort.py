# bench/counting_sort.prg in Python: counting sort of 1,000,000 keys in 0 .. 65535 (the high bits of a linear
# congruential generator), stable through prefix sums into an output list, 6 times with different seeds; the order and
# a weighted checksum are checked.
from bench import report, start


def run():
    n, m, ok, total = 1000000, 65536, True, 0
    a, b, cnt = [0] * n, [0] * n, [0] * m
    for rep in range(1, 7):
        seed = rep + 20
        for i in range(n):
            seed = seed * 16807 % 2147483647
            a[i] = seed // 32768
        for k in range(m):
            cnt[k] = 0
        for i in range(n):
            cnt[a[i]] += 1
        pos = 0
        for k in range(m):
            i = cnt[k]
            cnt[k] = pos
            pos += i
        for i in range(n):
            k = a[i]
            b[cnt[k]] = k
            cnt[k] += 1
        for i in range(1, n):
            if b[i - 1] > b[i]:
                ok = False
        for i in range(n):
            total += b[i] * ((i + 1) % 8 + 1)
    return ok and total == 884633960242


t = start()
report("counting_sort", t, run())
