# bench/dgemm.prg in Python: C := alpha A B + beta C on 250 x 250 matrices of floats in flat row-major lists (loops in
# i, k, j order), the inputs from a linear congruential generator, 3 times; the check is the sum and the trace of C.
from bench import report, start


def run():
    n, alpha, beta, seed = 250, 1.5, 0.5, 7
    a, b, c = [0.0] * (n * n), [0.0] * (n * n), [0.0] * (n * n)
    for i in range(n * n):
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
        a[i] = seed / 1073741824.0 - 1.0
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
        b[i] = seed / 1073741824.0 - 1.0
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
        c[i] = seed / 1073741824.0 - 1.0
    for _ in range(3):
        for i in range(n):
            ci = i * n
            for j in range(ci, ci + n):
                c[j] = beta * c[j]
            for k in range(n):
                aik = alpha * a[ci + k]
                bk = k * n - ci
                for j in range(ci, ci + n):
                    c[j] += aik * b[bk + j]
    total, trace = 0.0, 0.0
    for i in range(n):
        for j in range(n):
            total += c[i * n + j]
        trace += c[i * n + i]
    return abs(total + 145.62810535074) < 0.000001 and abs(trace - 342.80906789597) < 0.000001


t = start()
report("dgemm", t, run())
