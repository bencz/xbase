# bench/higher_order_chain.prg in Python: chains of lazy iterators over a list of 500,000 numbers, each step fed by the
# one before (the .prg's Enumerable methods as filter, map, itertools.islice, itertools.takewhile and sum): Where,
# Select, Skip, TakeWhile and Sum; Select with the position, Where, Take and Count; Distinct with a key and Count;
# 5 rounds. Python has no Distinct with a key or Count of an iterator: a generator over a set and a sum of ones.
from itertools import count, islice, takewhile

from bench import report, start


def distinct(it, key):
    seen = set()
    for x in it:
        k = key(x)
        if k not in seen:
            seen.add(k)
            yield x


def run(a):
    n = 0
    for r in range(1, 6):
        n += sum(takewhile(lambda x: x < 4900, islice(map(lambda x: x * r, filter(lambda x: x % 3 != 0, a)), 10, None)))
        n += sum(1 for _ in islice(filter(lambda x: x % 2 == 0, map(lambda x, pos: x + pos % 7, a, count(1))),
                                   50000 + r))
        n += sum(1 for _ in distinct(a, lambda x: x % (100 * r)))
    return n


a = [(i * 37) % 1000 for i in range(1, 500001)]
t = start()
report("higher_order_chain", t, run(a) == 1663633050)
