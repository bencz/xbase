# bench/mandelbrot.prg in Python: the Mandelbrot set as a bitmap (the benchmarks game's form), 1,600 x 1,600 points,
# at most 50 iterations each, one bit per point packed into bytes (a row padded to a whole byte), the bytes folded
# into a checksum.
from bench import report, start


def run():
    n, cs, n_bytes = 1600, 0, 0
    for y in range(n):
        ci = 2.0 * y / n - 1.0
        bits = 0
        n_bits = 0
        for x in range(n):
            cr = 2.0 * x / n - 1.5
            zr = zi = tr = ti = 0.0
            k = 0
            while k < 50 and tr + ti <= 4.0:
                zi = 2.0 * zr * zi + ci
                zr = tr - ti + cr
                tr = zr * zr
                ti = zi * zi
                k += 1
            bits = (bits << 1) | (1 if tr + ti <= 4.0 else 0)
            n_bits += 1
            if n_bits == 8 or x == n - 1:
                bits = bits << (8 - n_bits)
                cs = (cs * 31 + bits) & 0x7FFFFFFF
                n_bytes += 1
                bits = 0
                n_bits = 0
    return cs == 366763831 and n_bytes == n * ((n + 7) // 8)


t = start()
report("mandelbrot", t, run())
