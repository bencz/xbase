# bench/ini_parse.prg in Python: INI files, a text of 400 sections of 25 keys (comments, blank lines, spaces around "=",
# quoted values, section names in either case) made from a fixed seed, then timed: parsed 80 times into a dict of
# sections, each a dict of keys (lines split with split(), trimmed, names lowered, quotes taken off); the numbers
# summed and values looked up. The count of quoted values the .prg's ParseIni() sets by reference is returned with
# the sections.
from bench import report, start

seed = 7777


def rand(n):
    global seed
    seed = seed * 16807 % 2147483647
    return seed % n


def parse_ini(c):
    ini, section, quoted = {}, None, 0
    for line in c.split("\n"):
        line = line.strip(" ")
        if line == "" or line[0] in ";#":
            continue
        if line[0] == "[":
            section = {}
            ini[line[1:len(line) - 1].lower()] = section
        elif (eq := line.find("=") + 1) > 0:
            key = line[:eq - 1].rstrip(" ").lower()
            val = line[eq:].lstrip(" ")
            if len(val) >= 2 and val[0] == '"' and val[-1] == '"':
                val = val[1:len(val) - 1]
                quoted += 1
            section[key] = val
    return ini, quoted


def make():
    lines = []
    for s in range(1, 401):
        lines.append("; section " + str(s))
        lines.append("[" + ("Section" if rand(2) == 1 else "SECTION") + str(s) + "]")
        for k in range(1, 26):
            sp = rand(3)
            n = str(rand(100000))
            val = n if k % 5 == 0 else ('"text ' + n + '"' if k % 5 == 1 else "w" + n)
            lines.append(("  " if sp == 2 else "") + "Key" + "%02d" % k + (" = " if sp > 0 else "=") + val)
            if k % 7 == 0:
                lines.append("# comment")
        lines.append("")
    return "".join(x + "\n" for x in lines)


def run(c):
    ok = True
    for _ in range(80):
        h, quoted = parse_ini(c)
        total = 0
        for x in h.values():
            for k in range(5, 26, 5):
                total += int(x["key" + "%02d" % k])
        ok = (ok and len(h) == 400 and total == 102092980 and quoted == 2000
              and h["section77"]["key11"] == "text 72557" and len(h["section400"]) == 25)
    return ok and len(c) == 181694


text = make()
t = start()
report("ini_parse", t, run(text))
