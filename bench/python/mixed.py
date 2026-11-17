# bench/mixed.prg in Python: integers and doubles in one expression, double to integer, comparisons across both.
from bench import report, start


def run():
    s, n = 0, 0
    for i in range(1, 20000001):
        d = i + 3.14
        s += int(d * 4) % 7
        if d > i + 3:
            n += 1
        s += i / 4
    return s, n


t = start()
s, n = run()
report("mixed", t, s == 50000062499998 and n == 20000000)
