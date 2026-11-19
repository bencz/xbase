# bench/shared_counter.prg in Python: a counter shared by 8 threads, contended: each thread adds 1 to an atomic integer
# 250,000 times with add, then 50,000 times with a compare-exchange loop, then 50,000 times to a list element under a
# lock. Python has no atomic integers: AtomicInt is a class whose add and compare_exchange hold a lock of their own.
# No increment may be lost, whatever the scheduling.
import threading

from bench import report, start


class AtomicInt:
    def __init__(self):
        self.value = 0
        self._lock = threading.Lock()

    def add(self, n):
        with self._lock:
            self.value += n
            return self.value

    def compare_exchange(self, expected, new):
        with self._lock:
            if self.value == expected:
                self.value = new
                return True
            return False


def work(atomic, cas, lock, box):
    for _ in range(250000):
        atomic.add(1)
    for _ in range(50000):
        while True:
            n = cas.value
            if cas.compare_exchange(n, n + 1):
                break
    for _ in range(50000):
        with lock:
            box[0] += 1


def run():
    atomic, cas, lock, box = AtomicInt(), AtomicInt(), threading.Lock(), [0]
    threads = [threading.Thread(target=work, args=(atomic, cas, lock, box)) for _ in range(8)]
    for th in threads:
        th.start()
    for th in threads:
        th.join()
    return atomic.value == 8 * 250000 and cas.value == 8 * 50000 and box[0] == 8 * 50000


t = start()
report("shared_counter", t, run())
