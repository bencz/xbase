# bench/csv_parse.prg in Python: CSV parsed character by character, a 40,000-line text (about 2 MB) of 5 fields, some
# quoted with commas, doubled quotes and line breaks inside, read by a state machine into a list of records; numbers
# read with float() (the .prg's Val(); its Rand() numbers print with two decimals, "12345.00"); 6 rounds. The text is a
# list of lines joined (PyPy copies a string on every +=).
from bench import report, start

seed = 1999


def rand(n):
    global seed
    seed = (seed * 16807) % 2147483647
    return seed % n


def parse_csv(c):
    recs, rec, field, quoted, i, n_len = [], [], "", False, 0, len(c)
    while i < n_len:
        x = c[i]
        if quoted:
            if x == '"':
                if c[i + 1:i + 2] == '"':
                    field += x
                    i += 1
                else:
                    quoted = False
            else:
                field += x
        elif x == '"':
            quoted = True
        elif x == ",":
            rec.append(field)
            field = ""
        elif x == "\n":
            rec.append(field)
            recs.append(rec)
            rec = []
            field = ""
        else:
            field += x
        i += 1
    return recs


def make_field():
    k = rand(6)
    w = chr(97 + rand(26)) * (3 + rand(12))
    if k == 0:
        return '"' + w + ", " + w + '"'
    elif k == 1:
        return '"say ""' + w + '"""'
    elif k == 2:
        return '"' + w + "\n" + w + '"'
    return w


def make():
    lines = []
    for i in range(1, 40001):
        lines.append(str(i) + "," + make_field() + "," + "%.2f" % rand(100000) + "," + make_field() + ","
                     + "w" * rand(10) + "\n")
    return "".join(lines)


def run(c):
    n = quoted = 0
    recs = None
    for _ in range(6):
        recs = parse_csv(c)
        for a in recs:
            n += float(a[0]) + float(a[2]) + len(a[1]) + len(a[3]) + len(a)
            if "\n" in a[3] or '"' in a[1]:
                quoted += 1
    return len(recs) == 40000 and n == 16750579326 and quoted == 73038


data = make()
t = start()
report("csv_parse", t, run(data))
