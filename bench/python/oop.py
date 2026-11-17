# bench/oop.prg in Python: method calls (one calling its parent's through super), attribute reads and assignments in a
# loop of 3,000,000.
from bench import report, start


class TBase:
    def __init__(self):
        self.total = 0

    def add(self, x):
        self.total += x


class TCounter(TBase):
    def __init__(self):
        super().__init__()
        self.n = 0

    def add(self, x):
        super().add(x)

    def result(self):
        return self.total + self.n


def run():
    o = TCounter()
    for i in range(1, 3000001):
        o.add(i % 7)
        o.n = o.n + 1
    return o.result()


t = start()
report("oop", t, run() == 11999997)
