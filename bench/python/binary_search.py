# bench/binary_search.prg in Python: binary search written out, 1,000,000 keys from a fixed-seed generator looked for
# in a sorted list of 1,000,000 numbers (every third number), about a third of them found. The positions are the
# .prg's, from 1.
from bench import report, start

seed = 4711


def rand(n):
    global seed
    seed = (seed * 16807) % 2147483647
    return seed % n


def run(a):
    found = pos_sum = 0
    for _ in range(1000000):
        k = rand(3000010)
        lo, hi = 1, len(a)
        while lo <= hi:
            mid = (lo + hi) // 2
            if a[mid - 1] < k:
                lo = mid + 1
            elif a[mid - 1] > k:
                hi = mid - 1
            else:
                found += 1
                pos_sum += mid
                break
    return found == 333181 and pos_sum == 166512579113


a = [i * 3 for i in range(1, 1000001)]
t = start()
report("binary_search", t, run(a))
