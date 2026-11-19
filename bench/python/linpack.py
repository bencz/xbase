# bench/linpack.prg in Python: LINPACK, a dense 300 x 300 system (the original's matrix generator) solved by LU
# factorization with partial pivoting (DGEFA, DGESL, through DAXPY, IDAMAX and DSCAL on one column-major list), 5 times;
# the check is the solution, all ones, and the residual of A x - b. The lists keep the .prg's 1-based indexes
# (element 0 unused).
from bench import report, start


def mat_gen(a, b, n):
    init = 1325
    for j in range(1, n + 1):
        for i in range(1, n + 1):
            init = (3125 * init) % 65536
            a[(j - 1) * n + i] = (init - 32768.0) / 16384.0
    for i in range(1, n + 1):
        b[i] = 0.0
    for j in range(1, n + 1):
        for i in range(1, n + 1):
            b[i] += a[(j - 1) * n + i]


def dgefa(a, n, ipvt):
    for k in range(1, n):
        col = (k - 1) * n
        l = idamax(n - k + 1, a, col + k) + k - 1
        ipvt[k] = l
        if a[col + l] != 0.0:
            if l != k:
                a[col + l], a[col + k] = a[col + k], a[col + l]
            t = -1.0 / a[col + k]
            dscal(n - k, t, a, col + k + 1)
            for j in range(k + 1, n + 1):
                t = a[(j - 1) * n + l]
                if l != k:
                    a[(j - 1) * n + l] = a[(j - 1) * n + k]
                    a[(j - 1) * n + k] = t
                daxpy(n - k, t, a, col + k + 1, a, (j - 1) * n + k + 1)
    ipvt[n] = n


def dgesl(a, n, ipvt, b):
    for k in range(1, n):
        l = ipvt[k]
        t = b[l]
        if l != k:
            b[l] = b[k]
            b[k] = t
        daxpy(n - k, t, a, (k - 1) * n + k + 1, b, k + 1)
    for k in range(n, 0, -1):
        b[k] = b[k] / a[(k - 1) * n + k]
        t = -b[k]
        daxpy(k - 1, t, a, (k - 1) * n + 1, b, 1)


def daxpy(n, da, dx, x, dy, y):
    if n > 0 and da != 0.0:
        for i in range(n):
            dy[y + i] += da * dx[x + i]


def dscal(n, da, dx, x):
    for i in range(x, x + n):
        dx[i] = da * dx[i]


def idamax(n, dx, x):
    imax, dmax = 1, abs(dx[x])
    for i in range(2, n + 1):
        v = abs(dx[x + i - 1])
        if v > dmax:
            imax = i
            dmax = v
    return imax


def run():
    n, ok = 300, True
    for _ in range(5):
        a, b, ipvt = [None] * (n * n + 1), [None] * (n + 1), [None] * (n + 1)
        mat_gen(a, b, n)
        x = b[:]
        dgefa(a, n, ipvt)
        dgesl(a, n, ipvt, x)
        mat_gen(a, b, n)
        max_err = resid = 0.0
        for i in range(1, n + 1):
            max_err = max(max_err, abs(x[i] - 1.0))
            s = -b[i]
            for j in range(1, n + 1):
                s += a[(j - 1) * n + i] * x[j]
            resid = max(resid, abs(s))
        ok = ok and max_err < 0.000000001 and resid < 0.000000001
    return ok


t = start()
report("linpack", t, run())
