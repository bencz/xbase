# bench/monte_carlo_pi.prg in Python: pi by the Monte Carlo method, 50,000,000 points of the unit square from a linear
# congruential generator (31 bits), the count of those inside the quarter circle.
from bench import report, start


def run():
    n, seed, inside = 50000000, 42, 0
    for _ in range(n):
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
        x = seed / 2147483648.0
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
        y = seed / 2147483648.0
        if x * x + y * y <= 1.0:
            inside += 1
    est = 4.0 * inside / n
    return inside == 39267523 and abs(est - 3.14159) < 0.01


t = start()
report("monte_carlo_pi", t, run())
