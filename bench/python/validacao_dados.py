# bench/validacao_dados.prg in Python: data validation, 250,000 records [CPF, e-mail, date text, amount text] made from
# a fixed seed, with errors mixed in (a wrong check digit, an e-mail with no "@", no dot or a bad character, impossible
# dates, a letter among the digits). Timed: each field validated by its rule (the CPF's two mod-11 check digits, the
# e-mail's parts and characters, the date read with strptime("%d/%m/%Y"), where the .prg uses CToD() under SET DATE
# BRITISH and SET CENTURY ON and tests for the empty date, the amount's digits and range) and the valid ones counted.
import datetime

from bench import report, start

seed = 8086
LOCAL_CHARS = "abcdefghijklmnopqrstuvwxyz0123456789._"
DOMAIN_CHARS = "abcdefghijklmnopqrstuvwxyz0123456789.-"


def rand(n):
    global seed
    seed = seed * 16807 % 2147483647
    return seed % n


def cpf_ok(c):
    d = c.replace(".", "").replace("-", "")
    s = 0
    if len(d) != 11:
        return False
    for i in range(11):
        if not d[i].isdigit():
            return False
    for i in range(9):
        s += (ord(d[i]) - 48) * (10 - i)
    if s * 10 % 11 % 10 != ord(d[9]) - 48:
        return False
    s = 0
    for i in range(10):
        s += (ord(d[i]) - 48) * (11 - i)
    return s * 10 % 11 % 10 == ord(d[10]) - 48


def email_ok(e):
    at = e.find("@") + 1
    if at < 2 or "@" in e[at:]:
        return False
    for i in range(at - 1):
        if e[i] not in LOCAL_CHARS:
            return False
    dom = e[at:]
    if len(dom) < 3 or "." not in dom or dom[0] == "." or dom[-1] == ".":
        return False
    for ch in dom:
        if ch not in DOMAIN_CHARS:
            return False
    return True


def date_ok(c):
    try:
        datetime.datetime.strptime(c, "%d/%m/%Y")
    except ValueError:
        return False
    return True


def amount_ok(c):
    for ch in c:
        if not ch.isdigit():
            return False
    n = int(c)
    return 1 <= n <= 999999


def make_record():
    d, s, mail, amt = [0] * 11, 0, "", ""
    for k in range(9):
        d[k] = rand(10)
    for k in range(9):
        s += d[k] * (10 - k)
    d[9] = s * 10 % 11 % 10
    s = 0
    for k in range(10):
        s += d[k] * (11 - k)
    d[10] = s * 10 % 11 % 10
    if rand(4) == 0:
        d[10] = (d[10] + 1) % 10
    cpf = ""
    for k in range(1, 12):
        cpf += str(d[k - 1]) + ("." if k == 3 or k == 6 else ("-" if k == 9 else ""))
    for _ in range(rand(8) + 3):
        mail += "abcdefghijklmnopqrstuvwxyz0123456789"[rand(36)]
    kind = rand(20)
    dom = ["mail.com", "example.com.br", "x.org"][rand(3)]
    if kind == 0:
        mail += dom
    elif kind == 1:
        mail += "@examplecom"
    elif kind == 2:
        mail += "!@" + dom
    else:
        mail += "@" + dom
    s = "%02d" % (rand(31) + 1) + "/" + "%02d" % (rand(12) + 1) + "/" + "%4d" % (1990 + rand(41))
    n_len = rand(7) + 1
    for _ in range(n_len):
        amt += str(rand(10))
    if rand(10) == 0:
        k = rand(n_len)
        amt = amt[:k] + "x" + amt[k + 1:]
    return [cpf, mail, s, amt]


def run(records):
    n_c = n_e = n_d = n_a = n_all = total = 0
    for r in records:
        c = cpf_ok(r[0])
        e = email_ok(r[1])
        d = date_ok(r[2])
        a = amount_ok(r[3])
        if a:
            total += int(r[3])
        n_c += 1 if c else 0
        n_e += 1 if e else 0
        n_d += 1 if d else 0
        n_a += 1 if a else 0
        n_all += 1 if c and e and d and a else 0
    return (n_c == 187295 and n_e == 212602 and n_d == 245457 and n_a == 192603 and n_all == 120445
            and total == 19490512431)


data = [make_record() for _ in range(250000)]
t = start()
report("validacao_dados", t, run(data))
