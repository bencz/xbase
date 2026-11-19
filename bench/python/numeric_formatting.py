# bench/numeric_formatting.prg in Python: number formatting, 80,000 numbers with three decimals (-50,000 to 50,000)
# and integers (0 to 999,999) from a fixed seed, each formatted as the .prg's Str (width and decimals), StrZero, Round,
# and Transform with numeric pictures (thousands separators, @E, @( for negatives in parentheses, @Z) and @R on a
# string (a CPF mask) give them, and read back with float() (its Val). Python has no Str(), Transform() or xBase
# Round(): the texts come from format() with the pictures' rules written out (no minus sign without a non-zero digit
# shown, the parenthesized field's overflow as stars), and Round() is the runtime's half-away-from-zero formula. The
# text is a list of lines joined (PyPy copies a string on every +=). The text's length, its separators and the values
# read back are checked. No number lies halfway between two values of two decimals.
import math

from bench import report, start

seed = 314159
EPSILON = 2.220446049250313e-16
INTEGRAL = 4503599627370496.0
EUROPEAN = str.maketrans(",.", ".,")


def rand(n):
    global seed
    seed = seed * 16807 % 2147483647
    return seed % n


def xb_round(v, d):
    p = 10.0 ** d
    if v == 0.0:
        return 0.0
    if abs(v) * p >= INTEGRAL:
        return v
    tenfold = v * p * 10.0
    tenfold += -5.0 if tenfold < 0 else 5.0
    back = tenfold / 10.0
    return math.trunc(back + back * EPSILON) / p


def num(v, width, dec, sep=False):
    s = format(v, (",." if sep else ".") + str(dec) + "f")
    if s[0] == "-" and s.strip("-0.,") == "":
        s = s[1:]
    return s.rjust(width) if len(s) <= width else "*" * width


def parenthesized(v, template):
    if v >= 0:
        return num(v, len(template), 2, True)
    s = num(-v, len(template), 2, True)
    if s[0] != " ":
        s = "".join("*" if c == "9" else c for c in template)
    return "(" + s[1:] + ")"


def run():
    lines, cents, tenths = [], 0, 0
    for _ in range(80000):
        x = rand(100000000) - 50000000
        if abs(x) % 10 == 5:
            x += 1
        x /= 1000
        n = rand(1000000)
        z = "%011d" % n
        lines.append(num(x, 14, 2) + "%08d" % n + num(x, 10, 2, True).translate(EUROPEAN) + num(x, 11, 3, True)
                     + z[0:3] + "." + z[3:6] + "." + z[6:9] + "-" + z[9:11] + parenthesized(x, "99,999.99")
                     + (" " if n % 3 == 0 else "%d" % (n % 3)) + num(xb_round(x, 1), 12, 1).lstrip(" ") + "\n")
        cents += xb_round(float(num(x, 14, 2)) * 100, 0)
        tenths += xb_round(xb_round(x, 1) * 10, 0)
    c = "".join(lines)
    return (len(c) == 6065015 and c.count(",") == 236856 and c.count(".") == 558428 and c.count("(") == 41318
            and cents == -6496080269 and tenths == -649608444
            and c[:75] == "      35103.0200005610 35.103,02 35,103.019000.000.056-1035,103.02 35103.0\n")


t = start()
report("numeric_formatting", t, run())
