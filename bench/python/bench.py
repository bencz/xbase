# Shared by the Python benchmarks: the clock and the result line the xBase benchmarks print.
import time


def start():
    return time.perf_counter()


def report(name, t, ok):
    print(name, int((time.perf_counter() - t) * 1000), "ok" if ok else "WRONG")
