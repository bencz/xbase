# bench/upper_lower_alltrim.prg in Python: case and blanks, upper(), lower(), strip(), lstrip() and rstrip() (of
# blanks, as the .prg's AllTrim(), LTrim() and RTrim()) of 1,000 mixed-case strings padded with blanks on both sides,
# 1,500,000 times. The word lengths come from a loop that draws its limit again at every test, as the .prg's FOR does.
from bench import report, start

seed = 8


def rand(n):
    global seed
    seed = (seed * 16807) % 2147483647
    return seed % n


def make():
    a = [None] * 1000
    for i in range(len(a)):
        w, j = "", 1
        while j <= 5 + rand(30):
            w += "aBcDeFgHiJkLmNoPqRsTuVwXyZ 0123456789"[rand(37)]
            j += 1
        a[i] = " " * rand(6) + "x" + w + "Q" + " " * rand(6)
    return a


def run(a):
    n = 0
    for i in range(1, 1500001):
        x = a[i % 1000]
        w = x.strip(" ")
        n += len(w) + ord(w.upper()[0]) + ord(w.lower()[-1]) + len(x.rstrip(" ")) - len(x.lstrip(" "))
    return n == 320344500


strings = make()
t = start()
report("upper_lower_alltrim", t, run(strings))
