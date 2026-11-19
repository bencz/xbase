# bench/string_builder.prg in Python: a large text built by appending, 3 rounds of 400,000 table rows of about 37
# bytes appended to one string buffer (about 15 MB each), its length and every 1,000th character checked. The .prg
# appends with += to a string; the port writes to an io.StringIO, Python's string builder (PyPy copies a string on
# every +=, which would not finish).
import io

from bench import report, start

NAMES = ["ant", "bee", "cat", "dog", "eel", "fox", "gnu", "hen", "ibis", "jay"]


def run():
    n = 0
    for r in range(1, 4):
        buf = io.StringIO()
        for i in range(1, 400001):
            buf.write("<tr><td>" + NAMES[i % 10] + "</td><td>" + str(i + r) + "</td></tr>\n")
        c = buf.getvalue()
        n += len(c)
        for i in range(0, len(c), 1000):
            n += ord(c[i])
    return n == 47588053


t = start()
report("string_builder", t, run())
