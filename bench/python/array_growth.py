# bench/array_growth.prg in Python: many lists growing and shrinking, 300,000 lists grown with append() to 1 to 64
# elements, enlarged to twice that, cut to half, an element inserted in front and removed (the length kept, as the
# .prg's AIns() and ADel() keep it: the last element dropped, a None appended); one in 100 kept to the end.
from bench import report, start

seed = 3


def rand(n):
    global seed
    seed = (seed * 16807) % 2147483647
    return seed % n


def run():
    kept, n = [], 0
    for i in range(1, 300001):
        a = []
        size = rand(64) + 1
        for j in range(1, size + 1):
            a.append(j * 2)
        a.extend([None] * size)
        a[size * 2 - 1] = i % 1000
        n += a[size * 2 - 1]
        del a[(size + 1) // 2:]
        a.insert(0, None)
        a.pop()
        a[0] = size
        del a[0]
        a.append(None)
        a[-1] = i % 7
        n += len(a) + a[-1] + (a[0] if len(a) > 1 else 0)
        if i % 100 == 0:
            kept.append(a)
    for a in kept:
        n += a[0]
    return n == 156283982 and len(kept) == 3000


t = start()
report("array_growth", t, run())
