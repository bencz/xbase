# bench/float.prg in Python: Mandelbrot iterations over a 250 x 250 grid, at most 100 per point, 6 times.
from bench import report, start


def run():
    size, max_iter, total = 250, 100, 0
    for _ in range(6):
        for y in range(size):
            ci = y * 2.0 / size - 1.0
            for x in range(size):
                cr = x * 3.0 / size - 2.0
                zr = zi = 0.0
                k = 0
                while k < max_iter:
                    zr2, zi2 = zr * zr, zi * zi
                    if zr2 + zi2 > 4.0:
                        break
                    zi = 2.0 * zr * zi + ci
                    zr = zr2 - zi2 + cr
                    k += 1
                total += k
    return total


t = start()
report("float", t, run() == 11391384)
