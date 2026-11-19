# bench/array_append.prg in Python: lists grown one element at a time, 5 lists of 2,000,000 numbers built with append()
# from empty, then summed.
from bench import report, start


def run():
    total, a = 0, None
    for r in range(1, 6):
        a = []
        for i in range(1, 2000001):
            a.append(i + r)
        for x in a:
            total += x
    return total == 10000035000000 and len(a) == 2000000


t = start()
report("array_append", t, run())
