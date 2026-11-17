# bench/matmul.prg in Python: the product of two 100 x 100 integer matrices, 20 times.
from bench import report, start


def run():
    n = 100
    a = [[(i + j) % 7 for j in range(n)] for i in range(n)]
    b = [[(i * j) % 5 for j in range(n)] for i in range(n)]
    c = [[0] * n for _ in range(n)]
    total = 0
    for _ in range(20):
        for i in range(n):
            for j in range(n):
                s = 0
                for k in range(n):
                    s = s + a[i][k] * b[k][j]
                c[i][j] = s
                total += s
    return total


t = start()
report("matmul", t, run() == 20 * 4798200)
