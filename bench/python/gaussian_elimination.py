# bench/gaussian_elimination.prg in Python: floats in nested lists, a 300 x 300 system of random floats (a fixed seed)
# whose solution is all ones, solved by Gaussian elimination with partial pivoting (rows swapped as references) and
# back substitution, 6 times.
from bench import report, start


def solve(n):
    seed = 42
    a = [[None] * (n + 1) for _ in range(n)]
    for i in range(n):
        s = 0.0
        for j in range(n):
            seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
            a[i][j] = seed / 2147483648 - 0.5
            s = s + a[i][j]
        a[i][n] = s
    for k in range(n):
        p = k
        for i in range(k + 1, n):
            if abs(a[i][k]) > abs(a[p][k]):
                p = i
        piv = a[p]
        a[p] = a[k]
        a[k] = piv
        for i in range(k + 1, n):
            row = a[i]
            f = row[k] / piv[k]
            for j in range(k, n + 1):
                row[j] = row[j] - f * piv[j]
    x, err = [None] * n, 0.0
    for i in range(n - 1, -1, -1):
        row = a[i]
        s = row[n]
        for j in range(i + 1, n):
            s = s - row[j] * x[j]
        x[i] = s / row[i]
        err = max(err, abs(x[i] - 1.0))
    return err < 0.000000001


t = start()
report("gaussian_elimination", t, all([solve(300) for _ in range(6)]))
