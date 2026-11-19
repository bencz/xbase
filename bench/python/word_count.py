# bench/word_count.prg in Python: word frequencies, 200,000 lines of 1 to 30 words (about 1,300,000 words, of a
# 5,000-word vocabulary in mixed case and a skewed distribution) cut at blanks with find() and slices (the .prg's At()
# and SubStr()), lower-cased and counted in a dict; the most frequent word found. The word and line lengths come from
# loops that draw their limit again at every test, as the .prg's FOR does.
from bench import report, start

seed = 4242


def rand(n):
    global seed
    seed = (seed * 16807) % 2147483647
    return seed % n


def make():
    vocab, lines = [None] * 5000, [None] * 200000
    for i in range(len(vocab)):
        w, j = "", 1
        while j <= 2 + rand(9):
            w += "eEtaAoinNsrhldcumfpgwybvkxjqz"[rand(rand(29) + 1)]
            j += 1
        vocab[i] = w
    for i in range(len(lines)):
        c, j = "", 1
        while j <= 1 + rand(30):
            c += vocab[rand(rand(5000) + 1)] + " "
            j += 1
        lines[i] = c
    return vocab, lines


def run(vocab, lines):
    h, words, top, c_top, check = {}, 0, 0, "", 0
    for c in lines:
        while (n := c.find(" ") + 1) > 0:
            w = c[:n - 1].lower()
            c = c[n:]
            words += 1
            if w in h:
                h[w] += 1
            else:
                h[w] = 1
    for n in h.values():
        check += n * n
        if n > top:
            top = n
    for v in vocab:
        if h[v.lower()] == top:
            c_top = v.lower()
            break
    return words == 1312183 and len(h) == 4328 and top == 7336 and c_top == "ee" and check == 971695061


vocabulary, text_lines = make()
t = start()
report("word_count", t, run(vocabulary, text_lines))
