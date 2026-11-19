# bench/closure_capture.prg in Python: closures, 500,000 counters made by a function, each a closure holding its own
# copy of a local and a parameter, each called 4 times; then 2,000,000 calls of closures that share one captured
# local and change it.
from bench import report, start


def counter(step):
    count = 0

    def block(x):
        nonlocal count
        count += step + x
        return count
    return block


def run():
    n, shared = 0, 0
    a = [counter(i % 10) for i in range(1, 500001)]
    for j in range(1, 5):
        for b in a:
            n += b(j)

    def inc(x):
        nonlocal shared
        shared += x
        return shared

    def read():
        return shared % 1000

    for i in range(1, 2000001):
        inc(i % 7)
        n += read()
    return n == 1031500997 and shared == 5999997


t = start()
report("closure_capture", t, run())
