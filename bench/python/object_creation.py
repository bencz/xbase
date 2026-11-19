# bench/object_creation.prg in Python: creating objects, 600,000 objects of a class with five attributes (numbers, a
# string and a list, which each object gets its own of) made with an __init__ that sets two of them, half of them of a
# subclass with two more attributes; one in 100 kept in a list, the rest left to the collector.
import datetime

from bench import report, start

DUE = datetime.date(2026, 12, 31)


class TOrder:
    def __init__(self, n_id, customer):
        self.id = 0
        self.customer = ""
        self.total = 0
        self.status = "new"
        self.lines = []
        self.id = n_id
        self.customer = customer


class TBigOrder(TOrder):
    def __init__(self, n_id, customer):
        super().__init__(n_id, customer)
        self.discount = 0
        self.due = DUE


def run():
    keep, n = [], 0
    for i in range(1, 600001):
        if i % 2 == 0:
            o = TOrder(i, "c" + "%03d" % (i % 1000))
        else:
            o = TBigOrder(i, "c000")
            o.discount = 5
        o.lines.append(i % 7)
        if i % 100 == 0:
            keep.append(o)
    for o in keep:
        n += o.id + o.total + len(o.lines) + o.lines[0] + len(o.customer)
    return len(keep) == 6000 and n == 1800300000 + 6000 * 5 + 17999


t = start()
report("object_creation", t, run())
