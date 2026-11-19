# bench/ray_tracer.prg in Python: small lists of floats made and dropped, function calls, a 320 x 240 image of five
# spheres ray traced (diffuse light, shadows, reflections three deep), vectors as lists of three floats, checked by the
# sum of the pixels' gray levels, 2 times.
import math

from bench import report, start


def vadd(a, b):
    return [a[0] + b[0], a[1] + b[1], a[2] + b[2]]


def vsub(a, b):
    return [a[0] - b[0], a[1] - b[1], a[2] - b[2]]


def vscale(a, s):
    return [a[0] * s, a[1] * s, a[2] * s]


def vdot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def vnorm(a):
    return vscale(a, 1.0 / math.sqrt(vdot(a, a)))


def intersect(s, origin, d):
    oc = vsub(origin, s[0])
    b = vdot(oc, d)
    disc = b * b - (vdot(oc, oc) - s[1] * s[1])
    if disc > 0.0:
        q = math.sqrt(disc)
        t = -b - q
        if t < 0.0001:
            t = -b + q
        if t > 0.0001:
            return t
    return 0.0


def trace(spheres, light, origin, d, depth):
    best, hit = 0.0, None
    for s in spheres:
        t = intersect(s, origin, d)
        if t > 0.0 and (hit is None or t < best):
            best = t
            hit = s
    if hit is None:
        return 0.2 + 0.5 * max(0.0, d[1])
    p = vadd(origin, vscale(d, best))
    n = vscale(vsub(p, hit[0]), 1.0 / hit[1])
    to_light = vsub(light, p)
    dist = math.sqrt(vdot(to_light, to_light))
    l = vscale(to_light, 1.0 / dist)
    diff = vdot(n, l)
    for s in spheres:
        if diff <= 0.0:
            break
        t = intersect(s, p, l)
        if 0.0 < t < dist:
            diff = 0.0
    col = hit[3] * (0.1 + 0.9 * max(diff, 0.0))
    if depth < 3 and hit[2] > 0.0:
        col = col * (1.0 - hit[2]) + hit[2] * trace(spheres, light, p, vsub(d, vscale(n, 2.0 * vdot(d, n))),
                                                     depth + 1)
    return col


def run():
    width, height, ok = 320, 240, True
    spheres = [[[0.0, -1000.0, 0.0], 999.0, 0.2, 0.6], [[0.0, 0.0, 5.0], 1.0, 0.5, 0.9],
               [[-2.0, 0.0, 6.0], 1.0, 0.3, 0.7], [[2.0, 0.0, 6.0], 1.0, 0.3, 0.8],
               [[0.0, 2.2, 7.0], 1.0, 0.6, 0.5]]
    light, eye = [5.0, 10.0, -5.0], [0.0, 0.5, -1.0]
    for _ in range(2):
        total = 0
        for y in range(height):
            for x in range(width):
                total += int(min(trace(spheres, light, eye, vnorm([(x - width / 2.0 + 0.5) / height,
                                                                  (height / 2.0 - y - 0.5) / height, 1.0]), 0),
                                 1.0) * 255.0)
        ok = ok and total == 7100994
    return ok


t = start()
report("ray_tracer", t, run())
