# bench/fib.prg in Python: recursive calls, Fib( 37 ).
from bench import report, start


def fib(n):
    return n if n < 2 else fib(n - 1) + fib(n - 2)


t = start()
report("fib", t, fib(37) == 24157817)
