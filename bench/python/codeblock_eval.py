# bench/codeblock_eval.prg in Python: lambdas called, 10,000,000 calls of each of three lambdas kept in a list (no
# argument, two arguments, one that reads a captured local), the results summed.
from bench import report, start


def blocks(base):
    return [lambda: 7, lambda x, y: x * 2 + y, lambda x: x + base]


def run():
    n, b = 0, blocks(3)
    for i in range(1, 10000001):
        n += b[0]() + b[1](i % 100, 1) + b[2](i % 10)
    return n


t = start()
report("codeblock_eval", t, run() == 1145000000)
