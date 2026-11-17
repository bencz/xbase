# bench/tasks.prg in Python: 200,000 short tasks on a thread pool joined, a 50,000-link chain of tasks each waiting for
# the one before, and 20,000 tasks that each start and wait for another.
from concurrent.futures import ThreadPoolExecutor
import os
from bench import report, start


def run():
    with ThreadPoolExecutor(max_workers=os.cpu_count()) as pool:
        s = sum(f.result() for f in [pool.submit(lambda i: i % 7, k) for k in range(1, 200001)])
        c = pool.submit(lambda: 0)
        for _ in range(50000):
            c = pool.submit(lambda p: p.result() + 1, c)
        s += c.result()
        for _ in range(20000):
            s += pool.submit(lambda: pool.submit(lambda: 1).result()).result()
    return s


t = start()
report("tasks", t, run() == 599997 + 50000 + 20000)
