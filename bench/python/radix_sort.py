# bench/radix_sort.prg in Python: least-significant-digit radix sort, four passes of one byte each, of 300,000 31-bit
# integers from a linear congruential generator, between two lists, 10 times with different seeds; the order and a
# weighted checksum are checked.
from bench import report, start


def run():
    n, ok, total = 300000, True, 0
    a, b, cnt = [0] * n, [0] * n, [0] * 256
    for rep in range(1, 11):
        seed = rep + 30
        for i in range(n):
            seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
            a[i] = seed
        for d in range(4):
            shift = d * 8
            for k in range(256):
                cnt[k] = 0
            for i in range(n):
                cnt[(a[i] >> shift) & 0xFF] += 1
            pos = 0
            for k in range(256):
                x = cnt[k]
                cnt[k] = pos
                pos += x
            for i in range(n):
                x = a[i]
                k = (x >> shift) & 0xFF
                b[cnt[k]] = x
                cnt[k] += 1
            a, b = b, a
        for i in range(1, n):
            if a[i - 1] > a[i]:
                ok = False
        for i in range(n):
            total += a[i] * (((i + 1) & 7) + 1)
    return ok and total == 14501540638159929


t = start()
report("radix_sort", t, run())
