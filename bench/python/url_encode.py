# bench/url_encode.prg in Python: percent-encoding written out, 20,000 strings of 20 to 80 characters (letters, digits,
# blanks, punctuation and characters above 127) encoded as in a URL query (unreserved characters kept, a blank as +,
# any other as %XX) and decoded back; 8 rounds. The string lengths come from a loop that draws its limit again at
# every test, as the .prg's FOR does.
from bench import report, start

seed = 80
HEX_DIGITS = "0123456789ABCDEF"
CHARS = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJ0123456789     -_.~&=/?%:#+" + chr(233) + chr(252)


def rand(n):
    global seed
    seed = (seed * 16807) % 2147483647
    return seed % n


def url_encode(c):
    out = ""
    for x in c:
        if x in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.~":
            out += x
        elif x == " ":
            out += "+"
        else:
            n = ord(x)
            out += "%" + HEX_DIGITS[n // 16] + HEX_DIGITS[n % 16]
    return out


def url_decode(c):
    out, i, n = "", 0, len(c)
    while i < n:
        x = c[i]
        if x == "%":
            out += chr(HEX_DIGITS.find(c[i + 1]) * 16 + HEX_DIGITS.find(c[i + 2]))
            i += 3
        else:
            out += " " if x == "+" else x
            i += 1
    return out


def make():
    a = [None] * 20000
    for i in range(len(a)):
        c, j = "", 1
        while j <= 20 + rand(61):
            c += CHARS[rand(len(CHARS))]
            j += 1
        a[i] = c
    return a


def run(a):
    n, ok = 0, True
    for _ in range(8):
        for c in a:
            enc = url_encode(c)
            n += len(enc) + ord(enc[-1])
            ok = ok and url_decode(enc) == c
    return ok and n == 18744664 and url_encode("a b&c=" + chr(233)) == "a+b%26c%3D%E9"


strings = make()
t = start()
report("url_encode", t, run(strings))
