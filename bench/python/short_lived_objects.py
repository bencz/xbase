# bench/short_lived_objects.prg in Python: objects that die at once, 2,000,000 small vectors of a class, each added to a
# running sum by a method that returns a new vector, so three objects are made and as many dropped per step.
from bench import report, start


class TVec:
    def __init__(self, x, y):
        self.x = x
        self.y = y

    def add(self, o):
        return TVec(self.x + o.x, self.y + o.y)

    def scaled(self, k):
        return TVec(self.x * k, self.y * k)

    def dot(self, o):
        return self.x * o.x + self.y * o.y


def run():
    s, n = TVec(0, 0), 0
    for i in range(1, 2000001):
        v = TVec(i % 10, i % 7 - 3)
        s = s.add(v)
        n += v.dot(s.scaled(0)) + v.x
    return s.x == 9000000 and s.y == -3 and n == 9000000


t = start()
report("short_lived_objects", t, run())
