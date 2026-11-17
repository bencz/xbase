# bench/async.prg in Python: a million coroutines that complete at once, awaited; a hundred thousand round trips
# between two coroutines over queues of one; ten thousand handlers suspended at once on sleeps (asyncio).
import asyncio
from bench import report, start


async def sync(k):
    return k


async def echo(a, b, n):
    for _ in range(n):
        await b.put(await a.get())


async def ping_pong(n):
    a, b = asyncio.Queue(1), asyncio.Queue(1)
    t = asyncio.ensure_future(echo(a, b, n))
    s = 0
    for k in range(1, n + 1):
        await a.put(k)
        s += await b.get()
    await t
    return s


async def handler(k):
    await asyncio.sleep((k % 10) / 1000)
    await asyncio.to_thread(lambda: None)  # TaskRun( {|| NIL } ): a trip to a worker thread and back
    return k


async def main():
    t = start()
    s = 0
    for k in range(1, 1000001):
        s += await sync(k)
    s += await ping_pong(100000)
    s += sum(await asyncio.gather(*(handler(k) for k in range(1, 10001))))
    report("async", t, s == 500000500000 + 5000050000 + 50005000)


asyncio.run(main())
