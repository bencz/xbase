# bench/method_call_overhead.prg in Python: what a method call costs, 5,000,000 rounds of calls with no argument, one
# and three arguments, a one-line method (the .prg's INLINE) and a method that calls another through self; on an object
# made directly, then on one made by name through eval() (the .prg's macro). In Python every call is looked up at run
# time, so the two objects cost the same.
from bench import report, start


class TCalc:
    def __init__(self):
        self.base = 1

    def zero(self):
        return self.base

    def one(self, x):
        return x

    def three(self, x, y, z):
        return x + y + z - 5

    def twice(self, x):
        return x + x

    def chain(self, x):
        return self.one(x) + self.one(x) + 1


def run(o, u):
    n = 0
    for i in range(1, 5000001):
        n += o.zero() + o.one(i) + o.three(i, 2, 3) + o.twice(i) + o.chain(i)
        n += u.zero() + u.one(i) + u.three(i, 2, 3) + u.twice(i) + u.chain(i)
    return n


t = start()
report("method_call_overhead", t, run(TCalc(), eval("TCalc()")) == 150000050000000)
