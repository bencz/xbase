# bench/numerical_integration.prg in Python: functions called with floats, Simpson's rule over 1,000,000 intervals for
# four integrals whose values are known (4 / (1 + x^2) and sqrt(x) on [0, 1], exp(-x^2) on [0, 1], 1 / x on [1, 2]),
# each function a lambda (the .prg's codeblocks), 3 times.
import math

from bench import report, start


def simpson(f, a, b, n):
    h = (b - a) / n
    s = f(a) + f(b)
    for i in range(1, n):
        s = s + (4.0 if (i & 1) == 1 else 2.0) * f(a + i * h)
    return s * h / 3.0


def run():
    n, ok = 1000000, True
    for _ in range(3):
        r1 = simpson(lambda x: 4.0 / (1.0 + x * x), 0.0, 1.0, n)
        r2 = simpson(lambda x: math.exp(-(x * x)), 0.0, 1.0, n)
        r3 = simpson(lambda x: 1.0 / x, 1.0, 2.0, n)
        r4 = simpson(lambda x: math.sqrt(x), 0.0, 1.0, n)
        ok = (ok and abs(r1 - 3.141592653589793) < 0.000000000001 and abs(r2 - 0.746824132812427) < 0.000000000001
              and abs(r3 - 0.6931471805599453) < 0.000000000001 and abs(r4 - 2.0 / 3.0) < 0.000000001)
    return ok


t = start()
report("numerical_integration", t, run())
