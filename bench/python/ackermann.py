# bench/ackermann.prg in Python: deep recursion, Ackermann's function A(3, 9) (11 million calls, 4093 levels deep),
# 3 times.
import sys

from bench import report, start

sys.setrecursionlimit(20000)


def ack(m, n):
    if m == 0:
        return n + 1
    elif n == 0:
        return ack(m - 1, 1)
    return ack(m - 1, ack(m, n - 1))


t = start()
report("ackermann", t, all([ack(3, 9) == 4093 for _ in range(3)]))
