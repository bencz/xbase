# bench/async_generator.prg in Python: a pipeline of three asynchronous generators (a source of 1,000,000 numbers that
# yields to the event loop every 1000, a filter of the even ones, a map that doubles them), consumed by async for.
import asyncio
from bench import report, start


async def source(n):
    for k in range(1, n + 1):
        if k % 1000 == 0:
            await asyncio.sleep(0)
        yield k


async def evens(g):
    async for x in g:
        if x % 2 == 0:
            yield x


async def doubled(g):
    async for x in g:
        yield x * 2


async def main():
    t = start()
    s = 0
    async for x in doubled(evens(source(1000000))):
        s += x
    report("async_generator", t, s == 500001000000)


asyncio.run(main())
