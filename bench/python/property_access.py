# bench/property_access.prg in Python: attribute access, 3,000,000 rounds on an object made by name through eval() (the
# .prg's macro) of attribute reads and assignments (with =, += and += 1), a computed property (the .prg's ACCESS) and a
# property setter (its ASSIGN), and the same through a local name for the object (Python has no WITH OBJECT).
from bench import report, start


class TAccount:
    def __init__(self):
        self.balance = 0
        self.count = 0
        self.n_limit = 0
        self.last = ""

    @property
    def doubled(self):
        return self.balance * 2 - self.count - 2

    def _set_limit(self, x):
        self.n_limit = x * 10

    limit = property(fset=_set_limit)


def run(o):
    n = 0
    for i in range(1, 3000001):
        o.balance += 2
        o.count += 1
        o.last = "d"
        n += o.balance - o.count + o.doubled
        o.limit = i % 10
        w = o
        w.balance -= 1
        n += w.n_limit + len(w.last)
    return n == 4500001500000 + 10 * 13500000 + 2 * 3000000 and o.balance == 3000000


t = start()
report("property_access", t, run(eval("TAccount()")))
