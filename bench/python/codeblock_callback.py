# bench/codeblock_callback.prg in Python: functions that take a function, Simpson's rule over 3 functions given as
# lambdas (1,000,000 intervals each), and an insertion sort of 3,000 numbers that compares through a lambda, 10 times.
from bench import report, start

seed = 1


def rand(n):
    global seed
    seed = (seed * 16807) % 2147483647
    return seed % n


def simpson(f, a, b, n):
    h = (b - a) / n
    total = f(a) + f(b)
    for i in range(1, n):
        total += f(a + i * h) * (4 if i % 2 == 1 else 2)
    return total * h / 3


def insertion_sort(a, less):
    for i in range(1, len(a)):
        x = a[i]
        j = i - 1
        while j >= 0 and less(x, a[j]):
            a[j + 1] = a[j]
            j -= 1
        a[j + 1] = x


def run(data):
    n_int, check = 0, 0
    n_int += simpson(lambda x: x * x, 0, 3, 1000000)
    n_int += simpson(lambda x: 1 / (1 + x * x), 0, 1, 1000000)
    n_int += simpson(lambda x: x * x * x - 2 * x, -1, 2, 1000000)
    for r in range(1, 11):
        a = data[:]
        insertion_sort(a, (lambda x, y: x < y) if r % 2 == 0 else (lambda x, y: x > y))
        check += a[0] + a[1499] * 2 + a[2999] * 3
    return abs(n_int - 10.535398163397) < 0.000000001 and check == 2991620


data = [rand(100000) for _ in range(3000)]
t = start()
report("codeblock_callback", t, run(data))
