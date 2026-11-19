# bench/virtual_dispatch.prg in Python: virtual dispatch, 1,000 shapes of 6 classes that override area() and
# perimeter() of their parent TShape (made from a fixed seed, in random order), gone through 1,000 times: each shape's
# area() and perimeter() from one call site, and measure(), which TShape alone has and which calls the overriding
# area() through self.
from bench import report, start

seed = 6502


def rand(n):
    global seed
    seed = seed * 16807 % 2147483647
    return seed % n


class TShape:
    def __init__(self, x, y):
        self.x = x
        self.y = y

    def area(self):
        return 0

    def perimeter(self):
        return 0

    def measure(self):
        return self.area() + 1


class TCircle(TShape):
    def area(self):
        return 3 * self.x * self.x

    def perimeter(self):
        return 6 * self.x


class TSquare(TShape):
    def area(self):
        return self.x * self.x

    def perimeter(self):
        return 4 * self.x


class TRect(TShape):
    def area(self):
        return self.x * self.y

    def perimeter(self):
        return 2 * (self.x + self.y)


class TTriangle(TShape):
    def area(self):
        return self.x * self.y * 2 / 2

    def perimeter(self):
        return 3 * self.x


class THexagon(TShape):
    def area(self):
        return 6 * self.x * self.x

    def perimeter(self):
        return 6 * self.x


class TEllipse(TShape):
    def area(self):
        return 3 * self.x * self.y

    def perimeter(self):
        return 3 * (self.x + self.y)


def make():
    classes = [TCircle, TSquare, TRect, TTriangle, THexagon, TEllipse]
    shapes = []
    for _ in range(1000):
        cls = classes[rand(6)]
        x = rand(100) + 1
        shapes.append(cls(x, rand(100) + 1))
    return shapes


def run(shapes):
    area = per = m = 0
    for _ in range(1000):
        for o in shapes:
            area += o.area()
            per += o.perimeter()
            m += o.measure()
    return area == 1000 * 7844067 and per == 1000 * 245962 and m == 1000 * (7844067 + 1000)


shapes = make()
t = start()
report("virtual_dispatch", t, run(shapes))
