# bench/spectral_norm.prg in Python: the spectral norm of the infinite matrix A(i,j) = 1 / ((i+j)(i+j+1)/2 + i + 1)
# (the benchmarks game's), cut to 1,200 x 1,200: 10 rounds of the power method on A transposed times A, each element
# of A from a function call.
import math

from bench import report, start


def eval_a(i, j):
    return 1.0 / (int((i + j) * (i + j + 1) / 2) + i + 1)


def mul_av(n, u, v):
    for i in range(n):
        s = 0.0
        for j in range(n):
            s += eval_a(i, j) * u[j]
        v[i] = s


def mul_atv(n, u, v):
    for i in range(n):
        s = 0.0
        for j in range(n):
            s += eval_a(j, i) * u[j]
        v[i] = s


def mul_atav(n, u, v, w):
    mul_av(n, u, w)
    mul_atv(n, w, v)


def run():
    n, vbv, vv = 1200, 0.0, 0.0
    u, v, w = [1.0] * n, [0.0] * n, [0.0] * n
    for _ in range(10):
        mul_atav(n, u, v, w)
        mul_atav(n, v, u, w)
    for i in range(n):
        vbv += u[i] * v[i]
        vv += v[i] * v[i]
    return abs(math.sqrt(vbv / vv) - 1.2742241501) < 0.000000001


t = start()
report("spectral_norm", t, run())
