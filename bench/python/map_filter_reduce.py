# bench/map_filter_reduce.prg in Python: map, filter and reduce written out over lists with lambdas, a list of
# 1,000,000 numbers mapped, the result filtered and reduced to a sum; 8 rounds with other lambdas.
from bench import report, start


def map_list(a, f):
    out = [None] * len(a)
    for i in range(len(a)):
        out[i] = f(a[i])
    return out


def filter_list(a, f):
    out = []
    for x in a:
        if f(x):
            out.append(x)
    return out


def reduce_list(a, f, acc):
    for x in a:
        acc = f(acc, x)
    return acc


def run(a):
    n = 0
    for r in range(1, 9):
        n += reduce_list(filter_list(map_list(a, lambda x: x * r + 1), lambda x: x % 3 == 0), lambda acc, x: acc + x, 0)
    return n


a = [i % 1000 for i in range(1, 1000001)]
t = start()
report("map_filter_reduce", t, run(a) == 4492503000)
