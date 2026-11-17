# bench/async_pi.prg in Python: the Basel sum over 32 tasks of 50,000,000 terms each, on a pool of processes (threads
# would share one interpreter lock), joined in order; pi is Sqrt( 6 * sum ).
from concurrent.futures import ProcessPoolExecutor
import math
from bench import report, start

TERMS = 50000000


def partial(i):
    s = 0.0
    for n in range(i * TERMS + 1, (i + 1) * TERMS + 1):
        inv = 1.0 / n
        s += inv * inv
    return s


if __name__ == "__main__":
    t = start()
    with ProcessPoolExecutor() as pool:
        total = sum(pool.map(partial, range(32)))
    report("async_pi", t, abs(math.pi - math.sqrt(total * 6)) < 1e-9)
