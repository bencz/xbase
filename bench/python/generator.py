# bench/generator.prg in Python: a generator of 3,000,000 numbers and a second one that yields the even ones of the
# first.
from bench import report, start


def rng(a, b):
    for i in range(a, b + 1):
        yield i


def evens(e):
    for x in e:
        if x % 2 == 0:
            yield x


t = start()
s = 0
for x in evens(rng(1, 3000000)):
    s += x
report("generator", t, s == 2250001500000)
