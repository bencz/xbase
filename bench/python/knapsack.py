# bench/knapsack.prg in Python: 0/1 knapsack by dynamic programming over one list of capacities, 200 items (weights
# 1 .. 1000 and values 1 .. 1000 from a linear congruential generator) into capacities up to 10,000, 10 times with
# different seeds; the best values are checked.
from bench import report, start


def run():
    n, cap, total = 200, 10000, 0
    best, w, v = [0] * (cap + 1), [0] * n, [0] * n
    for rep in range(1, 11):
        seed = rep
        for i in range(n):
            seed = seed * 16807 % 2147483647
            w[i] = seed // 65536 % 1000 + 1
            seed = seed * 16807 % 2147483647
            v[i] = seed // 65536 % 1000 + 1
        for c in range(cap + 1):
            best[c] = 0
        for i in range(n):
            wi, vi = w[i], v[i]
            for c in range(cap, wi - 1, -1):
                x = best[c - wi] + vi
                if x > best[c]:
                    best[c] = x
        total += best[cap]
    return total


t = start()
report("knapsack", t, run() == 366516)
