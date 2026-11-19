# bench/aeval_heval.prg in Python: walks with a function, over a list of 1,000,000 numbers (the .prg's AEval()) and over
# a dict of 200,000 entries (its HEval()), the function adding to a captured local; 20 rounds. Python has no AEval() or
# HEval(): a for loop over enumerate() and over dict.items() calls the closure.
from bench import report, start


def run(a, h):
    n = 0
    for r in range(1, 21):
        def block_a(x, pos):
            nonlocal n
            n += x + (r if pos % 1000 == 0 else 0)

        def block_h(k, v):
            nonlocal n
            n += v + (1 if k % 3000 == 0 else 0)

        for pos, x in enumerate(a, 1):
            block_a(x, pos)
        for k, v in h.items():
            block_h(k, v)
    return n


a = [i % 777 for i in range(1, 1000001)]
h = {}
for i in range(1, 200001):
    h[i * 3] = i % 555
t = start()
report("aeval_heval", t, run(a, h) == 8867500260)
