# bench/hash_massivo.prg in Python: a dict with many keys, 1,000,000 numeric keys drawn from a fixed-seed generator
# counted in one dict (about 632,000 distinct), then 1,500,000 lookups of keys drawn from twice the range, about two
# thirds of them missing.
from bench import report, start

seed = 2024


def rand(n):
    global seed
    seed = (seed * 16807) % 2147483647
    return seed % n


def run():
    h, hits, total = {}, 0, 0
    for _ in range(1000000):
        k = rand(1000000)
        if k in h:
            h[k] += 1
        else:
            h[k] = 1
    for _ in range(1500000):
        k = rand(2000000)
        if k in h:
            hits += 1
            total += h[k]
    return len(h) == 631967 and hits == 473472 and total == 748930


t = start()
report("hash_massivo", t, run())
