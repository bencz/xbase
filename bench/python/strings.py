# bench/strings.prg in Python: Left, Upper, Len and At on 20,000,000 short strings.
from bench import report, start


def run():
    src, total = "abcdefghij", 0
    for i in range(1, 20000001):
        w = src[:i % 10 + 1].upper()
        total += len(w) + w.find("D") + 1
    return total


t = start()
report("strings", t, run() == 166000000)
