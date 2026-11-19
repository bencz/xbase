# bench/log_processing.prg in Python: log processing, 300,000 lines of an access log (time, level, [module], user,
# duration, status) made from a fixed seed as one text (the lines joined), then timed: split into lines with split(),
# each line parsed (slices at fixed columns, find() for the module and the key=value fields, int() of the digits the
# .prg's Val() reads), lines counted per level in a dict, durations summed per module, the slowest request found,
# server errors counted per hour, distinct users counted in a dict.
from bench import report, start

seed = 9001
LEVELS = ["INFO ", "WARN ", "ERROR", "DEBUG"]
MODULES = ["auth", "db", "http", "cache", "queue", "mail"]
STATUSES = [200, 200, 200, 201, 204, 301, 404, 500, 503, 200]


def rand(n):
    global seed
    seed = seed * 16807 % 2147483647
    return seed % n


def make(n):
    lines = [None] * n
    for i in range(1, n + 1):
        sec = i * 86400 // n
        x = rand(1000)
        lv = rand(10)
        lv = 1 if lv < 6 else (4 if lv < 8 else (2 if lv < 9 else 3))
        lines[i - 1] = ("2026-11-19 " + "%02d" % (sec // 3600) + ":" + "%02d" % (sec // 60 % 60) + ":"
                        + "%02d" % (sec % 60) + "." + "%03d" % x + " " + LEVELS[lv - 1] + " [" + MODULES[rand(6)]
                        + "] request done user=u" + str(rand(5000)) + " ms=" + str(rand(2000)) + " status="
                        + "%3d" % STATUSES[rand(10)])
    return "".join(x + "\n" for x in lines)


def run(c, n):
    h_lv, h_mod, h_users, hour_err = {}, {}, {}, [0] * 24
    high, high_line, err5, i = -1, 0, 0, 0
    for x in c.split("\n"):
        if x.strip() == "":
            continue
        i += 1
        lv = x[24:29].rstrip(" ")
        h_lv[lv] = h_lv.get(lv, 0) + 1
        nb = x.find("[") + 1
        mod = x[nb:x.find("]")]
        p_ms = x.find(" ms=")
        ms = int(x[p_ms + 4:x.find(" ", p_ms + 4)])
        h_mod[mod] = h_mod.get(mod, 0) + ms
        if ms > high:
            high = ms
            high_line = i
        st = int(x[x.find(" status=") + 8:])
        if st >= 500:
            err5 += 1
            hour_err[int(x[11:13])] += 1
        h_users[x[x.find(" user=") + 6:p_ms]] = True
    return (len(c) == 23765927 and i == n and h_lv["INFO"] == 179990 and h_lv["WARN"] == 29916
            and h_lv["ERROR"] == 30173 and h_lv["DEBUG"] == 59921 and h_mod["auth"] == 50005329
            and h_mod["db"] == 50599071 and h_mod["http"] == 50094170 and h_mod["cache"] == 49851623
            and h_mod["queue"] == 49898190 and h_mod["mail"] == 49855051 and high == 1999 and high_line == 1062
            and err5 == 59808 and hour_err[13] == 2503 and len(h_users) == 5000)


text = make(300000)
t = start()
report("log_processing", t, run(text, 300000))
