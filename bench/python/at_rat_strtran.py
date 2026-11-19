# bench/at_rat_strtran.prg in Python: searching and replacing, find() and rfind() (the .prg's At() and RAt(), from 1 and
# 0 when absent) of 200 words in a 20,000-byte text of those words, and replace() (its StrTran()) of each word by a
# longer one; 80 rounds. The word lengths come from a loop that draws its limit again at every test, as the .prg's
# FOR does.
from bench import report, start

seed = 33


def rand(n):
    global seed
    seed = (seed * 16807) % 2147483647
    return seed % n


def make():
    words, c = [None] * 200, ""
    for i in range(len(words)):
        w, j = "", 1
        while j <= 3 + rand(6):
            w += chr(97 + rand(26))
            j += 1
        words[i] = w
    while len(c) < 20000:
        c += words[rand(rand(200) + 1)] + " "
    return words, c


def run(words, c):
    n = 0
    for _ in range(80):
        for w in words:
            n += c.find(w) + 1 + c.rfind(w) + 1 + len(c.replace(w, "<" + w + ">"))
    return n


words, c = make()
t = start()
report("at_rat_strtran", t, run(words, c) == 620527280)
