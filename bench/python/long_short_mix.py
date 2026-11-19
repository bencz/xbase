# bench/long_short_mix.prg in Python: long-lived and short-lived values together, 100,000 records that live to the end,
# 2,500,000 temporary lists and strings, one in 8 of them stored into a long-lived record (an old value pointing to a
# new one), one record in 64 steps replaced by a new one. The .prg's Str( i ) is "%10d".
from bench import report, start

seed = 17


def rand(n):
    global seed
    seed = (seed * 16807) % 2147483647
    return seed % n


def run():
    long_lived, n = [[i, "%10d" % i, None] for i in range(1, 100001)], 0
    for i in range(1, 2500001):
        x = [i % 1000, chr(97 + i % 26) * (i % 40 + 1), None]
        n += len(x[1])
        if i % 8 == 0:
            k = rand(len(long_lived))
            long_lived[k][2] = x
        if i % 64 == 0:
            k = rand(len(long_lived))
            long_lived[k] = [long_lived[k][0] + 1, "%10d" % i, long_lived[k][2]]
    for r in long_lived:
        n += r[0] + len(r[1])
        if r[2] is not None:
            n += r[2][0]
    return n == 5099739142


t = start()
report("long_short_mix", t, run())
