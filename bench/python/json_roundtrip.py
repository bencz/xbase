# bench/json_roundtrip.prg in Python: JSON written and read back by code written out (as the .prg does; Python's json
# module is not used), 20,000 records as dicts (an id, a name with quotes, backslashes, tabs and line feeds, a list of
# tags, a boolean, a number, a null, a nested dict) made from a fixed seed, then timed: serialized to text, parsed back
# into dicts and lists by a recursive descent parser, and serialized again. The two texts must be equal, and the
# values read back add up as made. The serializer joins a list of pieces where the .prg appends to a string (PyPy
# copies a string on every +=), and the parser returns the position the .prg's Parse() updates by reference.
from bench import report, start

seed = 1618


def rand(n):
    global seed
    seed = seed * 16807 % 2147483647
    return seed % n


def to_json(x):
    parts = []
    write_json(x, parts)
    return "".join(parts)


def write_json(x, out):
    if isinstance(x, dict):
        out.append("{")
        sep = ""
        for k, v in x.items():
            out.append(sep + quote(k) + ":")
            write_json(v, out)
            sep = ","
        out.append("}")
    elif isinstance(x, list):
        out.append("[")
        sep = ""
        for v in x:
            out.append(sep)
            write_json(v, out)
            sep = ","
        out.append("]")
    elif isinstance(x, str):
        out.append(quote(x))
    elif isinstance(x, bool):
        out.append("true" if x else "false")
    elif isinstance(x, int):
        out.append(str(x))
    else:
        out.append("null")


def quote(c):
    return '"' + c.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n").replace("\t", "\\t") + '"'


def parse(c, pos):
    ch = c[pos:pos + 1]
    x = None
    if ch == "{":
        x = {}
        pos += 1
        if c[pos:pos + 1] == "}":
            return x, pos + 1
        while True:
            k, pos = parse_string(c, pos)
            pos += 1
            x[k], pos = parse(c, pos)
            ch = c[pos:pos + 1]
            pos += 1
            if ch == "}":
                break
    elif ch == "[":
        x = []
        pos += 1
        if c[pos:pos + 1] == "]":
            return x, pos + 1
        while True:
            v, pos = parse(c, pos)
            x.append(v)
            ch = c[pos:pos + 1]
            pos += 1
            if ch == "]":
                break
    elif ch == '"':
        x, pos = parse_string(c, pos)
    elif ch == "t":
        x = True
        pos += 4
    elif ch == "f":
        x = False
        pos += 5
    elif ch == "n":
        pos += 4
    else:
        k = pos
        while pos < len(c) and c[pos] in "-0123456789":
            pos += 1
        x = int(c[k:pos])
    return x, pos


def parse_string(c, pos):
    out = ""
    pos += 1
    begin = pos
    while (ch := c[pos]) != '"':
        if ch == "\\":
            out += c[begin:pos]
            ch = c[pos + 1]
            out += "\n" if ch == "n" else ("\t" if ch == "t" else ch)
            pos += 2
            begin = pos
        else:
            pos += 1
    out += c[begin:pos]
    return out, pos + 1


def make_record(i):
    name, tags, parent = "", [], None
    n = rand(11) + 5
    for _ in range(n):
        name += 'abcdefghijk ABC"\\\n\txyz'[rand(22)]
    n = rand(4)
    active = rand(2) == 1
    qty = rand(2001) - 1000
    if rand(3) != 0:
        parent = rand(i) + 1
    city = "City " + str(rand(50))
    zip_code = rand(90000) + 10000
    for _ in range(n):
        tags.append("tag" + str(rand(100)))
    return {"id": i, "name": name, "tags": tags, "active": active, "qty": qty, "parent": parent,
            "address": {"city": city, "zip": zip_code}}


def run(data):
    qty = active = tags = nulls = zips = 0
    c1 = to_json(data)
    back, pos = parse(c1, 0)
    c2 = to_json(back)
    for h in back:
        qty += h["qty"]
        active += 1 if h["active"] else 0
        tags += len(h["tags"])
        nulls += 1 if h["parent"] is None else 0
        zips += h["address"]["zip"]
    return (c1 == c2 and len(c1) == 2697804 and pos == len(c1) and len(back) == 20000 and qty == -88759
            and active == 9960 and tags == 29729 and nulls == 6525 and zips == 1098378340
            and back[0]["name"] == "ykAeACczkccifkx" and back[1]["name"] == 'Cdkk"\\Ae\nx\nz'
            and c1[:33] == '[{"id":1,"name":"ykAeACczkccifkx"')


records = [make_record(i) for i in range(1, 20001)]
t = start()
report("json_roundtrip", t, run(records))
