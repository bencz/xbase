# Concurrency: tasks, async/await and threads

*Planned*, except phase 1 (the platform's threads and synchronization,
[runtime.md](runtime.md#threads-and-synchronization)): the design the work follows, in the order of
[Phases](#phases). The goal is a runtime where one process serves hundreds of thousands of concurrent requests (a web
server with as many connections, each awaiting the network and the database), on every core, without a data race
ever corrupting memory, and where Clipper code that knows nothing of tasks keeps working inside them.

## Decisions

- **M:N, one heap, safe.** Tasks run on a pool of worker threads, one per core, with work stealing; a task may resume
  on another thread. There is one heap and one collector. The runtime guarantees that no race corrupts memory or
  crashes the process (see [Shared objects](#shared-objects)); a logical race (two tasks updating a counter) is the
  program's, which has mutexes, channels and atomics for it.
- **Stackless tasks.** An `ASYNC` function is compiled into a resumable function, the machinery generators already use
  ([xir.md](xir.md#resumable-functions)): its frame lives on the machine stack while it runs and is saved into a heap
  object only when it suspends. A suspended task costs its frame and a task object, nothing else: no stack of its
  own, so hundreds of thousands fit in memory, and a suspended task is an ordinary old object that young collections
  do not trace.
- **Explicit awaiting, safe blocking.** A suspension point is always written `AWAIT`. Code that blocks (a `SKIP` that
  reads the disk, a lock wait, a long loop) is never wrong inside a task: the scheduler sees the worker block and lets
  another thread serve the other tasks, as Go does with system calls. Awaitable forms exist where they pay off, the
  database operations first ([Database](#database)).
- **.NET's model, xBase's syntax.** `Task`, `AWAIT`, `CancellationToken`, `WhenAll`, `AWAIT FOR EACH`, `AWAIT USING`
  follow the .NET task model the enumerators already follow; names are xbase's own.

## Language

```xbase
ASYNC FUNCTION Handle( oConn )
   LOCAL oReq, aRows
   USING oDb := AWAIT DbConnectAsync( "orders" )
      DO WHILE ( oReq := AWAIT oConn:ReadRequestAsync() ) != NIL
         aRows := AWAIT oDb:QueryAsync( "customer", oReq:Param( "id" ) )
         AWAIT oConn:WriteAsync( Render( aRows ) )
      ENDDO
   END USING
   RETURN NIL                                   // oConn and oDb: classes of the libraries (phase 7)

ASYNC FUNCTION Lines( cFile )                  // an asynchronous generator
   LOCAL oFile := AWAIT FileOpenAsync( cFile ), cLine
   DO WHILE ( cLine := AWAIT oFile:ReadLineAsync() ) != NIL
      YIELD cLine
   ENDDO
   RETURN

AWAIT FOR EACH cLine IN Lines( "log.txt" )
   ? cLine
NEXT
```

- `ASYNC FUNCTION`, `ASYNC PROCEDURE`, `ASYNC METHOD` (and their `STATIC` forms): calling one runs it on the caller's
  thread until its first `AWAIT` of a task that has not completed, then returns a `Task` (a procedure's completes with
  `NIL`). One that never suspends returns a completed task.
- `AWAIT expr`: a unary operator, anywhere in an expression of an `ASYNC` function, also inside `BEGIN SEQUENCE`,
  `TRY`/`CATCH`/`FINALLY`, `ALWAYS`, `FOR EACH`, `WITH OBJECT`. A task gives its result, or raises its error in the
  awaiter (below); a value that is no task is its own result, so `AWAIT` of a function that may or may not be `ASYNC`
  is valid. `AWAIT` outside an `ASYNC` function is a compile error, except in the start procedure, which may await
  (the program then ends when it returns).
- Asynchronous generators: `YIELD` in an `ASYNC` function. Calling it returns an asynchronous enumerator
  (`MoveNextAsync()` gives a task of a logical, `Current`); `AWAIT FOR EACH x IN expr ... NEXT` awaits each step.
  `AWAIT FOR EACH` over an ordinary enumerator awaits each element that is a task (a collection of tasks in order).
- `USING [AWAIT] v := expr ... END [USING]`: the body runs, then `v:Dispose()` (with `AWAIT`, `AWAIT
  v:DisposeAsync()`) when it ends in any way (`RETURN`, `EXIT`, an error, `BREAK`), as `ALWAYS` would; `v` is a
  `LOCAL` of the statement. Classes of the runtime that hold resources (files, sockets, connections, locks) answer
  both.
- Errors: an error in a task goes to the task's error block, as in any code (it may substitute a value, retry, or
  `BREAK`). One that leaves the task's body (a `BREAK`, or the default handler that would end the program) completes
  the task as failed with the error object; `AWAIT` of a failed task raises it in the awaiter as `Break( oErr )`, so
  `RECOVER USING oErr` and `CATCH oErr` take it. A failed task nobody awaits is reported (on standard error, and to a
  handler the program may set) when the collector finds it unreachable. `QUIT` in any task ends the program.
- Restrictions, as for generators: no `AWAIT` in a codeblock (asynchronous codeblocks later), no `...`, `PCount()`,
  `PValue()` or `PARAMETERS` in an `ASYNC` function, and it creates no `PRIVATE` (whose dynamic scope would outlive a
  suspension); a function it calls creates them as usual.
- Commands: awaitable forms are `#command` rules in `std.ch` (`AWAIT SEEK x`, `AWAIT SKIP n`, `AWAIT COMMIT`), never
  special cases in the compiler.

## Runtime library

Classes and functions, listed in [coverage.md](coverage.md) as each one comes:

- `Task`: `Result` (waits), `Wait( [nMs] )`, `IsCompleted`, `IsFailed`, `IsCanceled`, `Error`, `ContinueWith( b )`;
  `TaskRun( bWork, ... )` (on the pool), `TaskDelay( nMs, [oToken] )`, `TaskYield()`, `TaskWhenAll( aTasks )`,
  `TaskWhenAny( aTasks )`, `TaskFromResult( x )`, `TaskTimeout( oTask, nMs )`.
- `TaskCompletionSource`: a task completed by code (callbacks, other threads).
- `CancellationTokenSource` / `CancellationToken`: cooperative cancellation, timeouts, linked tokens; every awaitable
  operation of the runtime takes one.
- `Channel( [nCapacity] )`: `SendAsync`, `ReceiveAsync`, `TrySend`, `TryReceive`, `Close`; enumerable with `AWAIT FOR
  EACH`. The way to hand data between tasks.
- Threads: `ThreadStart( bWork, ... )` (a task on a dedicated thread of its own, for work that blocks or computes for
  long), `Join()`; `Mutex` (`Lock`, `Unlock`, `TryLock`; `USING oMutex:Hold()`), `Condition`, `Semaphore` (also
  `WaitAsync`), `AsyncLock` (held across `AWAIT`), `AtomicInt` (`Add`, `CompareExchange`, `Value`), `Lazy( bInit )`.
- Statements of xbase's own over them, rules in `std.ch`: `SYNCHRONIZED oMutex ... END` (held for the body, released
  in any way it ends).

## Execution model

### Contexts and flows

Today `xb_ctx` is both the running state of the program's only thread and the program's state. It splits in two:

- The **thread context** (`xb_ctx`, one per worker or program thread): what compiled code uses (`pending`, the chain
  of frames on the machine stack, the C roots), per-thread caches (the macro cache), and a pointer to the current
  flow's environment.
- The **environment** of a flow of execution (`xb_env`): the `SET` values, `ErrorBlock`, the `PRIVATE` stack, the
  current work area number, `SET LANGUAGE` and `SET CODEPAGE`, `FError()`, the default RDD; and a reference to the
  flow's table of work areas. A worker running a task points its context at the task's environment; switching tasks
  is a pointer store. `SET DECIMALS`, which compiled code reads directly, moves from the context to the environment
  (an ABI change).

Environments are copied on write: sequential code shares one and sees every `SET` as Clipper does; where execution
forks (a task started with `TaskRun`, an `ASYNC` call that suspends while its caller goes on without awaiting it), both
sides keep the same environment until one of them changes it, which copies it first. So a callee's `SET` reaches its
caller when they run in sequence, never across concurrent branches. A new flow (`TaskRun`, `ThreadStart`) starts with
a copy of its creator's environment and a table of work areas of its own, empty; concurrent branches of one flow share
its table (each area serializes its operations) and each has its own current area.

`PUBLIC` variables are program-wide, in one shared table (read without locking, created under a lock); `PRIVATE`
variables belong to the environment. Module `STATIC`s and class variables are shared by every task and thread, and
are [shared objects](#shared-objects) themselves.

### Scheduler

- Workers: one per core by default (a runtime option of the command line sets it, `//WORKERS:n`); the program's start
  procedure runs as the first task.
- Each worker has a work-stealing deque (Chase-Lev) and a slot for the task it just woke (run next, for locality);
  tasks from outside the pool go to a global injection queue. An idle worker steals half of a random victim's deque,
  polls its I/O reactor and the timers, then parks.
- **Blocking handoff**: every operation that may block the thread (file and lock I/O, `Mutex` waits, `Wait()`,
  console input, a foreign call marked blocking) is bracketed by the runtime (`xb_blocking_enter` / `leave`). While a
  worker blocks with work waiting, another thread takes its place (started on demand, kept for a while, capped); the
  blocked one gives its place back when it returns. A monitor thread also sees a task that runs for long without a
  suspension point (a computation) and adds a thread the same way. Legacy synchronous code inside tasks therefore
  costs threads, never liveness.
- Timers: a hierarchical timing wheel per worker for `TaskDelay`, timeouts and cancellation after a delay; hundreds of
  thousands of pending timeouts cost constant time each.
- A blocking pool, separate from the workers, runs work that has no asynchronous form (`getaddrinfo`, file I/O where
  the system has no asynchronous files, foreign libraries).

### Tasks

- A task object holds its state (running, suspended, completed, failed, canceled), its result or error, its
  continuations (one inline, more in a list), its environment, its cancellation token and, while suspended, its frame
  and resume function.
- Completing a task schedules its continuations on the completing worker (run next) or, for one that must not run
  inline, on the pool. `AWAIT` of a completed task continues inline with no allocation.
- Cost targets (measured, [Testing and measurement](#testing-and-measurement)): an `AWAIT` of a completed task in tens
  of nanoseconds, a suspension and resumption in a few hundred, a suspended task in well under 1 KB beyond what its
  own variables hold.

### I/O reactor

One reactor per worker, polled when the worker has nothing to run and when it parks (with the next timer as the
timeout):

- Linux: io_uring (multishot accept and receive, buffers provided to the kernel so that an idle connection holds no
  read buffer, asynchronous file reads and writes); epoll where io_uring is unavailable or disabled, with
  `SO_REUSEPORT` listeners per worker.
- macOS and the BSDs: kqueue; file I/O on the blocking pool.
- Windows: one I/O completion port shared by the workers (`GetQueuedCompletionStatusEx`); files and sockets opened for
  overlapped I/O.
- Platform layer (`runtime/src/platform/`): sockets (TCP and UDP, IPv4 and IPv6, non-blocking, accept, connect, read,
  write, shutdown, options), the reactor, wake-ups between threads, positional asynchronous file I/O, monotonic time
  in nanoseconds; the portable part (reactor interface, timers, buffers) in `runtime/src/io/`.

## Shared objects

Items are 16 bytes, written as two words: a thread reading an item while another writes it could see the type of one
value and the pointer of the other. Growing an array or a hash while another thread reads it could leave the reader on
freed storage. The runtime rules both out:

- **Local and shared.** An object is *local* while only one flow can reach it, and *shared* once two can (a header bit,
  `XB_GC_HDR_SHARED`). Sharing is transitive: everything a shared object refers to is shared.
- **Publication.** A reference becomes reachable by another flow only at known places, and each one shares the object
  and what it reaches (once per object, like marking): a store into a shared object or into shared storage (module
  `STATIC`s, `PUBLIC`s, class variables, a channel, a task's result), the arguments and captured cells of a new flow
  (`TaskRun`, `ThreadStart`), and the frame of an `ASYNC` call that suspends while its caller goes on without awaiting
  it. The write barrier, which every store into a heap object already passes through, tests the stored-into object's
  header byte; the shared bit lives in the same byte, so local stores cost nothing more.
- **Access.** A shared object's items are read under its sequence counter (read, check, retry: no lock, no write) and
  written, grown and shrunk under its lock (a bit of the header, with a parking table for the contended case).
  Compiled code's in-place reads of arrays, hashes and object variables test the header bit first (one load and a
  branch, predicted) and take the shared path when it is set. Strings are immutable; a shared string never grows in
  place (its append flag is cleared when it is shared). Read-only operations that reorganize a hash (compaction, the
  position tree) run under the lock when it is shared.
- **Lifetime.** Conservative stack scanning keeps alive (not only pins) what any thread's stack points into, so storage
  a reader is still inside never goes away under it.
- **Runtime caches** that compiled code and the runtime fill at run time stop being multi-word: a send cache's way
  is one atomic pointer to an immutable record (class, member, slot), a field cache one 64-bit word (generation,
  index), the table generation an atomic counter, the memory variable name table grows in chunks published
  atomically, and a class is prepared once under a lock that knows the task preparing it (its nesting count moves
  into the context).

## Garbage collector

The collector ([runtime.md](runtime.md#garbage-collector)) gets, in this order:

1. **Stop the world.** A collection asks every running thread to stop (a bit of `pending` on each thread context, set
   atomically) and waits for each one to reach a safepoint: compiled code already tests `pending` after calls, and
   gets a test at loop back edges (an ABI change); long runtime loops (sorts, index builds, `PACK`) test it too. A
   thread in a blocking operation is already stopped: it published its stack top, with its registers saved, when it
   entered, and does not touch the heap until it has checked for a collection on its way back.
2. **Every stack.** The conservative scan covers every thread's stack, from its published top to its start, and keeps
   alive and pins what it finds. Suspended tasks have no stack: their frames are heap objects, traced precisely.
3. **Thread-safe mutators.** Header state bits change with atomic byte operations; the remembered set is filled through
   per-thread buffers drained at collection, not under the heap lock; allocation counts are per thread, summed
   atomically; destructors run on a finalizer task of their own, never in whichever task polls next.
4. **Parallel collection.** Marking with per-thread mark stacks and work stealing, sweeping lazily per block as
   allocation reaches it.
5. **Concurrent marking** (a snapshot-at-the-beginning barrier) once pauses under the server load of the benchmarks
   exceed their target, and per-request arenas from escape analysis (the GC direction's stage 4).

## Database

Work areas belong to a flow ([Contexts and flows](#contexts-and-flows)); the file protocol and the in-process lock
manager ([rdd.md](rdd.md#sharing-locks-and-concurrency)) already make two areas behave alike whether they are in one
task, in two threads or in two programs.

- **Synchronous operations** (`SKIP`, `SEEK`, `REPLACE`, `dbAppend()` and the rest) keep their meaning everywhere, and
  block only their thread ([Scheduler](#scheduler)).
- **Awaitable operations**, for every driver (DBF, NTX, CDX, FPT, DBT and the next ones): `dbUseAreaAsync()`,
  `dbGoToAsync()`, `dbSkipAsync()`, `dbSeekAsync()`, `dbAppendAsync()`, `dbCommitAsync()`, `RLockAsync( [nRecord],
  [nTimeout], [oToken] )`, `FLockAsync()`, `ordCreateAsync()`, `__dbPackAsync()`, with `AWAIT` forms of the commands
  in `std.ch`, and cursors as asynchronous enumerators (`AWAIT FOR EACH oRec IN dbRecordsAsync( [bFor], [bWhile] )`).
  Waiting for a lock becomes possible (Clipper's `RLock()` still answers at once): a lock held inside the process is
  awaited on a continuation of the lock manager, one held by another program by retries on the timers, with the
  timeout and the token.
- **First implementation**: an awaitable operation runs the driver's synchronous operation on the blocking pool as the
  task (its environment, its areas; the pool thread is a mutator like any), and the task's worker serves other tasks
  meanwhile. Correct for every driver at once, including keys, filters and conditions evaluated during the operation.
- **Then the fast path**: a page cache per file shared by the process (already planned for shared indexes), read
  ahead asynchronously. Navigation (`GoTo`, `Skip`, `Seek`) is computed from cached pages and committed at the end;
  when a page is missing it stops before changing anything, the task awaits the page's asynchronous read, and the
  operation runs again. An operation the cache serves completes inline with no thread handoff. Writes go to the area's
  write-behind buffers and are flushed asynchronously under the protocol's locks.
- SQLite (RDD step 7) runs its operations on the blocking pool, a connection per flow; network databases come as
  libraries speaking their protocols over the asynchronous sockets.

## Libraries

In `libs/` ([architecture.md](architecture.md#libraries-written-in-xbase)), on top of the reactor: `Socket` (TCP and
UDP, server and client, awaitable and blocking methods), an HTTP/1.1 server and client (parser and writer in C,
keep-alive, pipelining, chunked bodies, streaming; HTTP/2 later), WebSocket, TLS through the system's library (OpenSSL
on POSIX, Schannel on Windows, the system's on macOS), JSON, and clients for PostgreSQL and MySQL.

## Testing and measurement

- Regression tests in `tests/run` for every construct and function; results that do not depend on scheduling order.
  A runtime option runs the pool with one worker and a fixed order, so that a test may also check the interleaving.
- Stress tests (bounded in time and memory, [testing.md](testing.md)): a million short tasks, a hundred thousand
  suspended ones, channels in rings, `TaskWhenAll` fan-outs, collections during all of it, shared arrays and hashes
  written and read by every worker (the [shared objects](#shared-objects) guarantee).
- The runtime built with ThreadSanitizer for the scheduler, the collector and the shared-object paths, and with the
  address and undefined-behavior sanitizers.
- Benchmarks (`bench/`): the cost of `AWAIT` (completed and suspending), task creation, a channel's throughput, an echo
  server with 100,000 connections (memory per idle connection, the target under 2 KB beyond the kernel's buffers), an
  HTTP server's requests per second and its 99th-percentile latency with the collector running, a CPU-bound workload
  on threads compared with the reference's threads (never slower).

## Phases

Each phase ends with its tests, its benchmarks measured before and after, the docs updated and a commit.

1. **Platform** (done): threads (create, join, identity), condition variables, reader-writer locks, one-time
   initialization, atomics on ordinary fields (the compilers' builtins and intrinsics), waiting on an address (a
   parking lot), monotonic nanoseconds.
2. **A thread-safe runtime**: the context split and environments (ABI); stop-the-world collections with back-edge
   polls (ABI), every stack scanned, atomic headers, per-thread remembered sets, the finalizer task; the runtime caches
   and global state made thread-safe; shared objects (header bit, publication, sequence counters and locks, compiled
   fast paths); line-atomic console output; `ThreadStart`, `Mutex`, `Condition`, `Semaphore`, `AtomicInt`,
   `SYNCHRONIZED`. The first visible feature: threads sharing memory safely.
3. **Scheduler and tasks**: workers, deques, stealing, parking, blocking handoff and its monitor, timers, the
   blocking pool; `Task`, `TaskRun`, `TaskDelay`, `TaskWhenAll`/`Any`, `TaskCompletionSource`, cancellation,
   `Channel`, usable from synchronous code (`Wait()`, `ContinueWith`).
4. **The compiler**: `ASYNC` functions, procedures and methods, `AWAIT` everywhere in them, the start procedure
   awaiting, errors and cancellation across `AWAIT`, the completed fast path; asynchronous generators and `AWAIT FOR
   EACH`; `USING` and `AWAIT USING`.
5. **Asynchronous I/O**: the reactors (io_uring and epoll, kqueue, IOCP), sockets, asynchronous files, name resolution
   on the blocking pool; the echo server benchmark.
6. **The database**: awaitable operations on the blocking pool for every driver, lock waits with timeouts and tokens,
   cursors as asynchronous enumerators, the `AWAIT` commands; then the shared page cache, read-ahead and the inline
   fast path; SQLite (RDD step 7).
7. **Libraries**: HTTP server and client, WebSocket, TLS, JSON, database clients; the HTTP benchmark with 100,000
   connections.
8. **The collector at scale**: parallel marking and lazy sweeping, then concurrent marking and arenas, against the
   pause and memory targets of phase 7's benchmarks.
