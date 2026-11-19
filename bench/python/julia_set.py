# bench/julia_set.prg in Python: the Julia set of c = -0.8 + 0.156i, escape times over a 1,600 x 1,200 grid of
# [-1.5, 1.5] x [-1, 1], at most 255 iterations per point, counted in a histogram of 256 bins folded into a checksum.
from bench import report, start


def run():
    w, h, max_iter, cr, ci = 1600, 1200, 255, -0.8, 0.156
    hist, total, cs = [0] * (max_iter + 1), 0, 0
    for y in range(h):
        for x in range(w):
            zr = x * 3.0 / w - 1.5
            zi = y * 2.0 / h - 1.0
            k = 0
            while k < max_iter:
                zr2 = zr * zr
                zi2 = zi * zi
                if zr2 + zi2 > 4.0:
                    break
                zi = 2.0 * zr * zi + ci
                zr = zr2 - zi2 + cr
                k += 1
            hist[k] += 1
            total += k
    for k in range(max_iter + 1):
        cs = (cs * 31 + hist[k]) % 2147483648
    return total == 69243973 and cs == 2105097618


t = start()
report("julia_set", t, run())
