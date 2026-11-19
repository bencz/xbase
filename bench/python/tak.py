# bench/tak.prg in Python: recursive calls with three arguments, the Takeuchi function Tak(24, 16, 8), 10 times.
from bench import report, start


def tak(x, y, z):
    if y < x:
        return tak(tak(x - 1, y, z), tak(y - 1, z, x), tak(z - 1, x, y))
    return z


t = start()
report("tak", t, all([tak(24, 16, 8) == 9 for _ in range(10)]))
