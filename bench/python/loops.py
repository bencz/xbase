# bench/loops.prg in Python: the sum of the first 500,000,000 odd numbers.
from bench import report, start


def run():
    n = 0
    for i in range(1, 500000001):
        n = n + i * 2 - 1
    return n


t = start()
report("loops", t, run() == 250000000000000000)
