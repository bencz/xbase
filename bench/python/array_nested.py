# bench/array_nested.prg in Python: nested lists, a 100 x 100 x 100 cube, filled through a[i][j][k], then 10 passes
# that change the cells from their neighbors along the last two dimensions and sum them.
from bench import report, start


def run():
    a = [[[None] * 100 for _ in range(100)] for _ in range(100)]
    total = 0
    for i in range(100):
        for j in range(100):
            for k in range(100):
                a[i][j][k] = ((i + 1) * 31 + (j + 1) * 17 + (k + 1) * 7) % 101
    for _ in range(10):
        for i in range(100):
            plane = a[i]
            for j in range(1, 100):
                row = plane[j]
                for k in range(1, 100):
                    row[k] = (row[k] + plane[j - 1][k] + row[k - 1]) % 1000
        for i in range(100):
            for j in range(100):
                for k in range(100):
                    total += a[i][j][k]
    return total == 4905893027


t = start()
report("array_nested", t, run())
