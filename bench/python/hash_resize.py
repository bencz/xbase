# bench/hash_resize.prg in Python: a dict that grows and shrinks, 20 rounds of 100,000 keys added to an empty dict and
# deleted again, the even ones first, then the odd ones.
from bench import report, start


def run():
    total, ok = 0, True
    for r in range(1, 21):
        h = {}
        for i in range(1, 100001):
            h[i * 7 + r] = i
        ok = ok and len(h) == 100000
        for i in range(2, 100001, 2):
            total += h[i * 7 + r]
            del h[i * 7 + r]
        ok = ok and len(h) == 50000
        for i in range(1, 100001, 2):
            total += h[i * 7 + r]
            del h[i * 7 + r]
        ok = ok and len(h) == 0
    return ok and total == 100001000000


t = start()
report("hash_resize", t, run())
