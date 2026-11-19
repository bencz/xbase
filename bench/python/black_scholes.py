# bench/black_scholes.prg in Python: Black-Scholes prices of 500,000 European options (a call and a put each), their
# inputs from a linear congruential generator: the normal distribution by its polynomial approximation, exp, log and
# sqrt, the sum of the prices.
import math

from bench import report, start


def cnd(d):
    k = 1.0 / (1.0 + 0.2316419 * abs(d))
    w = 1.0 - 0.3989422804014327 * math.exp(-0.5 * d * d) * \
        k * (0.319381530 + k * (-0.356563782 + k * (1.781477937 + k * (-1.821255978 + k * 1.330274429))))
    return 1.0 - w if d < 0.0 else w


def run():
    seed, r, v, sum_call, sum_put = 12345, 0.02, 0.30, 0.0, 0.0
    for _ in range(500000):
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
        s = 5.0 + 25.0 * seed / 2147483648.0
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
        x = 1.0 + 99.0 * seed / 2147483648.0
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
        tm = 0.25 + 9.75 * seed / 2147483648.0
        vst = v * math.sqrt(tm)
        d1 = (math.log(s / x) + (r + v * v * 0.5) * tm) / vst
        d2 = d1 - vst
        disc = x * math.exp(-r * tm)
        sum_call += s * cnd(d1) - disc * cnd(d2)
        sum_put += disc * cnd(-d2) - s * cnd(-d1)
    return abs(sum_call / 1494483.92385431 - 1) < 0.000000001 and abs(sum_put / 15576648.71605097 - 1) < 0.000000001


t = start()
report("black_scholes", t, run())
