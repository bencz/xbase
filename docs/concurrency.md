# Concurrency: tasks, async/await and threads

*Planned*, except phase 1 (the platform's threads and synchronization,
[runtime.md](runtime.md#threads-and-synchronization)), phase 2 ([Threads](#threads)) and what phase 3 marks done
([Scheduler](#scheduler), [Tasks](#tasks)): the design the work follows, in the order of [Phases](#phases). The goal is
a runtime where one process serves hundreds of thousands of concurrent requests (a web server with as many connections,
each awaiting the network and the database), on every core, without a data race ever corrupting memory, and where
Clipper code that knows nothing of tasks keeps working inside them.

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

- `ASYNC FUNCTION`, `ASYNC PROCEDURE`, `ASYNC METHOD` (and their `STATIC` forms; a class declares `ASYNC METHOD name`
  and its implementation is `ASYNC METHOD` too, never `INLINE`): calling one runs it on the caller's thread until its
  first `AWAIT` of a task that has not ended, then returns a `Task` (a procedure's completes with `NIL`); its next
  steps run on the workers, each when what it awaits has ended. One that never suspends returns a task ended already
  (done, [Asynchronous functions](#asynchronous-functions)).
- `AWAIT expr`: a unary operator, anywhere in an expression of an `ASYNC` function (arguments, `.AND.`/`.OR.`, `IIF`,
  assignments, literals, loop conditions), also inside `BEGIN SEQUENCE` (`WITH` too), `TRY`/`CATCH`/`FINALLY`,
  `ALWAYS`, `FOR`, `FOR EACH`, `WITH OBJECT`, `USING`. A task gives its result, or raises its error in the awaiter
  (below); a value that is no task is its own result, so `AWAIT` of a function that may or may not be `ASYNC` is
  valid. `AWAIT` is the operator when an operand starts right after it (a name, a literal, `{`, `::`, a macro `&name`);
  `AWAIT(` is a call of a function `Await()`, as `YIELD(` is of `Yield()`, and the preprocessor's expression markers
  follow the same rule (`? AWAIT f( x ), y`). Outside an `ASYNC` function it is a compile error, except in a function
  that may start the program (a public `MAIN`, or the first function of its module), where it waits for the task; the
  start procedure may also be `ASYNC`: the runtime waits for the task it returns before the program ends, its failure
  raised as `Result` raises it, so an error nobody handled is reported and ends the program with exit code 1
  ([runtime.md](runtime.md#program-startup)). Done.
- `AWAIT FOR EACH [k =>] x IN expr ... NEXT` awaits each step of an asynchronous enumerator (an object that answers
  `MoveNextAsync()`, a task of a logical, and `Current`: a channel's enumerator, a class's; an object that answers
  `GetAsyncEnumerator()` gives one), a channel's values as they come too; over an ordinary collection (an array, a hash,
  a string, an enumerator, a generator) it awaits each element that is a task, in order (a collection of tasks); `k` is
  the position, a hash's key; the variables are put back after the loop, as `FOR EACH` puts them (a generator left at a
  `YIELD` in its body leaves them as they are, as with `FOR EACH`); a field cannot be one, and the statement cannot
  stand in a codeblock. The loop calls `MoveNextAsync()` without a token: one that should end it is given to the
  enumerator, as `AWAIT FOR EACH x IN oChannel:GetEnumerator( oToken )` (canceled, the step's `AWAIT` fails as canceled,
  and no value goes to the loop that was left) (done, [compiler.md](compiler.md#grammar)).
- Asynchronous generators: `YIELD` in an `ASYNC` function (or method). Calling one runs nothing and returns its
  enumerator (`AsyncGenerator`, [runtime.md](runtime.md#asynchronous-generators)): `MoveNextAsync( [oToken] )` gives a
  task of whether an element came, then in `Current`; the first one runs the body on the caller's thread, each later one
  runs its next step at once when the stack has room (else a worker does), up to a `YIELD` (`.T.`) or the end (`.F.`,
  and `.F.` from then on); an `AWAIT` in the body suspends it, the task still waiting; an error fails the task and ends
  the enumeration. One `MoveNextAsync()` at a time: another while one waits raises `BASE/9035`; its token cancels the
  wait and ends the enumeration (what the body yields then is dropped). `MoveNext()` waits (`FOR EACH`), and `AWAIT FOR
  EACH` awaits each step; `YIELD` may stand in an `AWAIT FOR EACH`'s body, so generators chain. Unlike a synchronous
  generator, the body may create `PRIVATE`s (its environment is its own, as any `ASYNC` function's). A generator left at
  a `YIELD` is collected whole, body and environment. Done.
- `[AWAIT] USING [v :=] expr ... END [USING]`: the body runs, then the value's `Dispose()` (with `AWAIT`, `AWAIT
  DisposeAsync()`; `USING AWAIT expr` is `USING` of the awaited value) when it ends in any way (`RETURN`, `EXIT`,
  `LOOP`, an error, `BREAK`, `QUIT`), as `ALWAYS` would. Without `AWAIT` it is done
  ([compiler.md](compiler.md#grammar)): the value is kept in a variable of the statement's own, so the one disposed is
  the one evaluated, a NIL value is not disposed, and `v`, when written, is an ordinary variable that the value is
  assigned to (an assignment that fails disposes the value too). Classes of the runtime that hold resources (files,
  sockets, connections, locks) answer both.
- Errors: an error in a task goes to the task's error block, as in any code (it may substitute a value, retry, or
  `BREAK`). One that leaves the task's body (a `BREAK`, or the default handler that would end the program) completes the
  task as failed with the error object; `AWAIT` of a failed task raises it in the awaiter as `Result` does
  ([Tasks](#tasks)): the Error object through the awaiter's error block (so `CATCH oErr` takes it, and so does `RECOVER
  USING oErr` under an error block that breaks), another value as `Break( value )` (done). The default handler reports
  an error that crossed tasks with where it happened, then each place it was waited for (`Called from INNER(29)`, then
  `Waited for in MIDDLE(24)`, `Waited for in MAIN(11)`, tests/run/async_errors; the task's own calls only, also for a
  step that ran inline: tests/run/async_errors_early: done). A failed task that nothing observed is reported when the
  collector finds it unreachable ([Tasks](#tasks), done). `QUIT` in any task ends the program.
- Restrictions: no `AWAIT` in a codeblock, no `...`, `PCount()`, `PValue()` or `PARAMETERS` in an `ASYNC` function
  (done). Its `PRIVATE` variables (a `PRIVATE` statement, or an assignment to a name no variable has) belong to its
  own environment: visible to what it calls, they last across `AWAIT` until it ends (done). *(Planned)*: asynchronous
  codeblocks; `...` and the arguments as received in `ASYNC` functions and generators.
- Locks belong to the flow that takes them: a thread, or an `ASYNC` function while one of its steps runs
  (`xb_flow_self`: a 64-bit number never given twice, so no flow ever takes another's lock for its own, however many
  flows a long-running process starts). So `SYNCHRONIZED oMutex` around an `AWAIT` keeps the mutex while the function is
  suspended and releases it from whatever worker runs its end, and another task that the same worker runs meanwhile
  waits for it as any other flow (done; C# forbids `await` inside `lock`). A wait for a held lock with `Lock()` blocks
  its worker (another takes its place); `AWAIT oMutex:LockAsync()`, `AWAIT SYNCHRONIZED oMutex` and `AWAIT
  oSemaphore:WaitAsync()` suspend the function instead (done, [Threads](#threads)).
- Commands: awaitable forms are `#command` rules in `std.ch` (`AWAIT SEEK x`, `AWAIT SKIP n`, `AWAIT COMMIT`), never
  special cases in the compiler.

## Runtime library

Classes and functions, listed in [coverage.md](coverage.md) as each one comes:

- `Task`: `Result` (waits), `Wait( [nMs] )`, `IsCompleted`, `IsFailed`, `IsCanceled`, `Error`, `ContinueWith( b )`;
  `TaskRun( bWork, ... )` (on the pool), `TaskWhenAll( aTasks )`, `TaskWhenAny( aTasks )`, `TaskFromResult( x )`:
  `TaskDelay( nMs, [oToken] )`, `TaskTimeout( oTask, nMs )`, `TaskYield()`: done ([Tasks](#tasks)).
- `TaskCompletionSource`: a task completed by code (callbacks, other threads): done ([Tasks](#tasks)).
- `CancellationTokenSource` / `CancellationToken`: cooperative cancellation, timeouts, linked tokens (done:
  [Tasks](#tasks)); the awaitable operations of the runtime take one (`TaskDelay`, a channel's; planned: the
  asynchronous waits of phase 4 and the I/O of phase 5).
- `Channel( [nCapacity] )`: `SendAsync`, `ReceiveAsync`, `TrySend`, `TryReceive`, `Close`, `Count`, `IsClosed`;
  enumerable with `FOR EACH` (done: [Channels](#channels)), and with `AWAIT FOR EACH` (phase 4). The way to hand
  data between tasks.
- Threads: `ThreadStart( bWork, ... )` (a flow on a thread of its own, for work that blocks or computes for long),
  `Join()`, `IsAlive()`; `Mutex()` (`Lock`, `Unlock`, `TryLock`, `Hold`); `Condition()` (`Wait`, `Notify`,
  `NotifyAll`); `Semaphore()` (`Wait`, `Release`, `Count`); `AtomicInt()` (`Add`, `CompareExchange`, `Value`); the
  asynchronous waits `Mutex`'s `LockAsync` and `HoldAsync` (held across `AWAIT`) and `Semaphore`'s `WaitAsync`: done,
  see [Threads](#threads). Planned: `Lazy( bInit )`; a thread becomes a task once tasks exist.
- Statements of xbase's own over them, rules in `std.ch`: `SYNCHRONIZED oMutex ... END` (held for the body, released
  in any way it ends; done).

## Execution model

### Contexts and flows

The context is split in two (done, [runtime.md](runtime.md#threads-and-synchronization)):

- The **thread context** (`xb_ctx`, one per worker or program thread): what compiled code uses (`pending`, the chain
  of frames on the machine stack, the stack limit, `SET DECIMALS`), the C roots, per-thread caches (the
  macro cache), and a pointer to the current flow's environment.
- The **environment** of a flow of execution (`xb_env`): the `SET` values, `ErrorBlock`, the memory variables, the
  current work area number, `SET LANGUAGE` and `SET CODEPAGE`, `FError()`, the default RDD, and the flow's work areas. A
  task runs in an environment of its own, made from the snapshot its creator's gave it, in front of the thread's (done).
  An `ASYNC` function has an environment of its own from its call to its end (done): at the call a copy of its caller's
  (the `SET` values, `ErrorBlock()`, the language, the code page, the default driver, the PRIVATE variables visible, the
  same variables) with no work area open; what it changes there (`SET`, `SELECT`, `USE`, `ErrorBlock()`) is its own,
  never its caller's, also in its first step, and lasts across `AWAIT`; its tables stay open across `AWAIT` and close
  when it ends. Between steps the environment is parked; one whose function is abandoned (what it awaits is dropped, so
  it never resumes) is closed after the collection that finds the task unreachable (at the next pending test of the
  context that runs destructors), its tables and locks with it: a young collection finds one parked a short while, one
  that outlived two young collections is found by the next full collection. *(Planned)*:
  `SET DECIMALS` moves into the environment, copied into the context when it changes, so that switching between
  suspended tasks is a pointer store and that copy.

Sequential code shares one environment and sees every `SET` as Clipper does. A new flow (`TaskRun`, `ThreadStart`, an
`ASYNC` call) starts with a copy of its creator's environment and a table of work areas of its own, empty (done): what
it changes never reaches its creator.

`PUBLIC` variables are program-wide, in one shared table (read without locking, created under a lock: done,
[runtime.md](runtime.md#memory-variables)); `PRIVATE` variables belong to the environment, and a new flow sees the ones
its creator sees, the same variables. Module `STATIC`s and class variables are shared by every task and thread, and
are [shared objects](#shared-objects) themselves.

### Threads

Done (`rtl/threads.c`, [runtime.md](runtime.md#threads-and-synchronization)); the functions are xbase's own.

- `ThreadStart( bWork, [xArgs...] )` runs `Eval( bWork, xArgs... )` on a new thread (a codeblock or a function
  reference; anything else is the argument error `BASE/9016`, and a thread the system cannot make `BASE/9017`) and
  returns its `Thread` object. The arguments are values (one written `@` passes its value), and they and the block
  are shared before the thread starts. `oThread:Join()` waits
  for the thread to end and returns what the block returned (`NIL` when a `BREAK` left it); a thread that joins itself
  gets `BASE/9018` `Thread joins itself` (substitutable). `oThread:IsAlive()` tells whether it still runs.
- The thread starts with a copy of its creator's environment: the `SET` values (`SET DECIMALS` too), `ErrorBlock()`,
  the language and the code page, the default driver, and the memory variables its creator sees: a `PRIVATE` visible
  then is the same variable in both (assigned in one, seen by the other); no work area is open. What the thread
  changes in its own environment (a `SET`, a `PRIVATE` it creates) stays there.
- An error goes to the thread's error block, the creator's: a block that `BREAK`s ends the thread with `NIL`; the
  default handler reports the error and quits. `QUIT` in any thread (an error nobody handles too) ends the program:
  every other thread is asked to `QUIT` (a thread waiting in `Join()`, `Lock()` or any other wait is woken by the
  request itself), the program waits for them, then runs its `EXIT` procedures, with the exit code `ErrorLevel()` set.
  The program also ends when its start procedure returns: the threads still running are asked to `QUIT` the same way and
  waited for. A thread started meanwhile quits at once; the `EXIT` procedures may start threads, which end the same way
  when they return. A thread inside a system call that does not return (a read of the standard input, a file lock
  another program never releases) holds the program's end until it returns. A thread that ends holding a `Mutex` leaves
  it held.
- `Mutex()` makes a lock that the flow holding it (a thread, a task, an `ASYNC` function: [Asynchronous
  functions](#asynchronous-functions)) may take again: `Lock()` waits while another flow holds it (a blocking operation:
  collections go on without the waiter), `TryLock()` takes it only when no other flow holds it and tells whether it did,
  `Unlock()` releases it once (it is free when released as many times as taken); `Unlock()` by a flow that does not hold
  it is `BASE/9019` `Mutex not held`. A wait for a mutex that a flow the waiter runs inside holds (an `ASYNC` function's
  first step, a task run inline, wanting what its caller holds) is `BASE/9031` `Mutex held by a flow this one runs in`:
  it could never end. `Hold()` takes it as `Lock()` does and returns a holder (`MutexHolder`) whose `Dispose()` releases
  it once, from the flow holding it (from another, it does nothing), for `USING`; NIL when a `QUIT` ended the wait or
  came as it took the lock (then released). `LockAsync( [nMs], [oToken] )` waits without holding a thread: a task that
  completes with `.T.` once the calling flow holds the lock (at once when it is free and no such waiter is in line, or
  the flow holds it already), with `.F.` when nMs milliseconds passed first (without nMs, NIL or a negative one: no
  limit), canceled by the token; an `ASYNC` function that awaits it holds the lock across its next steps and releases it
  with `Unlock()` from any of them. `HoldAsync( [nMs], [oToken] )` is the same wait completed with a holder, or NIL when
  the time passed. A release that frees the lock gives it to the first of these waiters that is still waiting (canceled
  and timed-out ones are passed over), in the order they came; a thread waiting with `Lock()` may take it before them. A
  limit that is no number, or a token that is no token, is `BASE/9024` `Argument error`.
- `SYNCHRONIZED oMutex ... END` (`END SYNCHRONIZED` too) runs the body holding the mutex and releases it however the
  body ends: a `std.ch` rule, `USING oMutex:Hold()` (`oMutex` is a variable, an element, a call or a message: an
  operator around it would bind before `:Hold()`). `RETURN`, `EXIT` and `LOOP` may leave it. `AWAIT SYNCHRONIZED
  oMutex ... END`, in an `ASYNC` function, awaits the mutex instead (`USING AWAIT oMutex:HoldAsync()`) and holds it
  across the `AWAIT`s of the body.
- `Condition()` makes a condition variable. `oCondition:Wait( oMutex, [nMs] )` releases `oMutex`, which the flow
  must hold (else `BASE/9021` `Mutex not held`; a limit that is no number, `BASE/9021` `Argument error`), however
  many times it took it, waits for a `Notify()` or `NotifyAll()` or nMs milliseconds (without nMs, NIL or a negative
  one: no limit), takes `oMutex` back as it held it, and tells whether it was notified. `Notify()` lets one waiting
  thread go on, `NotifyAll()` every one; a notification no thread waits for is lost, so a waiter tests its condition in
  a loop, holding the mutex. A `QUIT` ends the wait without the mutex. Up to 65535 threads wait on a condition at
  once; one more waits for a place with the mutex released, then takes it back and returns `.F.`, as if it had timed
  out.
- `Semaphore( nInitial, [nMaximum] )` counts free places (0 .. nMaximum, at most 2^31 - 1, its default; a value out of
  range, or nMaximum below nInitial, is `BASE/9022` `Argument error`). `Wait( [nMs] )` takes a place, waiting while none
  is free (nMs as `Wait()`'s above; one that is no number is `BASE/9022`; a `QUIT` ends the wait), and tells whether it
  took one; `Release( [nCount] )` gives back nCount places (1 without it; below 1, `BASE/9022`) and returns the count
  before, or, past the maximum, raises `BASE/9023` `Semaphore full` (`EG_LIMIT`) and changes nothing; `Count()` gives
  the places free. `WaitAsync( [nMs], [oToken] )` takes a place without holding a thread: a task that completes with
  `.T.` once it took one (at once when one is free and no such waiter is in line), with `.F.` when nMs milliseconds
  passed first, canceled by the token (its arguments as `LockAsync`'s); the places a `Release()` gives go to these
  waiters in their order (a thread waiting with `Wait()` may take one before them).
- `AtomicInt( [nValue] )` holds a 64-bit integer (0 without a value; a number is truncated toward zero, saturated):
  `Value` reads it, `Value := n` stores n, `Add( n )` adds n and returns the new value (wrapping as two's complement
  does), `CompareExchange( nExpected, nNew )` stores nNew when the value is nExpected and tells whether it did; each in
  one step whatever other threads do. An argument that is no number is `BASE/9020`.
- Console output: what one `QOut()`, `QQOut()`, `OutStd()` or `OutErr()` call writes, and the runtime's report of an
  unrecoverable error, is written whole: the outputs of threads never split one another
  ([runtime.md](runtime.md#functions-written-in-xbase)). The default error handler's report is several `OutErr()`
  calls: another thread's lines may come between them.

### Scheduler

Done (`core/sched.c`, [runtime.md](runtime.md#threads-and-synchronization)), but where marked planned:

- Workers: threads started when tasks first need them; one per processor runs tasks at once by default, `//WORKERS:n`
  on the command line sets it (an argument that starts with `//` is a runtime option, which the program does not
  receive). *(Planned)*: the program's start procedure runs as the first task; today it runs on the program's thread,
  which submits tasks to the pool and waits for them as any other thread (a task it returns is waited for at its end).
- Each worker has a queue of its own, a ring of 256 tasks it fills at the tail and takes from at the head, and a slot
  for the task it made last (run next, for locality); a thread outside the pool that submits tasks (the program's
  thread, `ThreadStart`'s) fills a ring of its own the same way, without a lock, which workers steal from as from one
  another (the ring serves another thread once its thread ends); the half a full ring sheds goes to the pool's queue,
  which a worker looks at first once every 61 tasks, taking its share of what waits. A worker that finds nothing steals
  half of another's ring (that worker's next task when its ring is empty and its task has run 5 µs: a task that ends
  soon runs the task it made ready itself, without a thread switch), searches 50 µs more while another worker runs (one
  searcher at a time), then parks; a submission wakes a parked worker, or starts one, while fewer are active than the
  target and none searches. The counters every worker changes (active, searching, queued and ended tasks) each sit on a
  cache line of their own (`XB_CACHE_LINE`, `core/sync.h`), so that changing one does not slow the readers of another.
- **Blocking handoff**: every operation that may block the thread (file and lock I/O, `Mutex` waits, `Wait()`, console
  input, a foreign call marked blocking) runs through the runtime's `xb_blocking_run`. While a worker running a task
  blocks with work waiting, another takes its place: an idle one woken, or a new one started, at once while fewer than
  twice the target are alive, past that paced (the next one 25 ms after the last, 25 ms more for each `target` workers
  beyond twice the target, at most 250 ms), so that tasks blocked for long cost a few threads a second, not a thread
  each; up to the most workers (1024, or as many as have their 4 MB stacks in a quarter of the address space the process
  may map, for a 32-bit process or one with a limit on it, so that the heap keeps the rest). `TaskWorkers()` gives `{
  alive, target, most }`. The blocked one is active again when it returns, and a worker beyond the target that stays
  idle 2 seconds ends. A thread that waits with no limit for a task still to start runs it itself (inline), while its
  stack has 1 MB left above the stack error's limit (`xb_stack_room`, `core/ctx.c`): past that it waits, and a worker
  runs the task on a stack of its own, so a chain of tasks that each wait for the next does not overflow one stack.
  One that waits for `TaskWhenAll()` runs the tasks of it still to start the same way, the newest first, until it
  ended: a fork-join, whose tasks each wait for the tasks they started, finishes with any number of workers, where its
  waits would otherwise hold every worker while the tasks they wait for stay in the queues. One that waits for
  `TaskWhenAny()` runs none: it would wait for the one it runs, when another may end first.
  Synchronous code inside tasks therefore costs threads, up to the most workers or the threads the system lets start;
  past that the tasks left wait in the queues until a worker comes back, which may be never when what the blocked tasks
  wait for is one of those left (a task that would release their lock, close their channel). The asynchronous waits
  (`LockAsync`, `WaitAsync`, `ReceiveAsync`, `AWAIT` of a task) hold no worker. When tasks wait in the queues, none
  ended for 2 seconds and then no worker is active (each one waits or runs one task long, or none could start), the
  program's standard error is told so once (`xbrt: ... tasks have waited ...`), until tasks end again: the program's
  throughput is held by tasks that hold their workers (more start only slowly), which is not a deadlock. A short wait
  for a lock of the runtime's (a stripe, a channel's lock: held a moment) runs through `xb_waiting_run` instead: the
  worker keeps its place, so many tasks meeting on one channel never multiply the threads. A monitor sees a task that
  runs long (20 ms) without blocking while others wait in the queues, and puts another worker beside it the same way; it
  also starts again the workers whose threads did not start.
- Timers (`TaskDelay`, `TaskTimeout`, `CancelAfter`): a binary heap of deadlines that one thread serves, started with
  the pool: it sleeps until the earliest deadline and fires what is due; it is also the monitor, looking every 10 ms
  while tasks wait in the queues (with nothing queued it sleeps until its next timer, and a submission that finds every
  worker busy wakes it). When the system gives it no thread, that is said once on standard error and the next timer
  added tries again; till then no timer fires, so `:Wait( nMs )` on a delay gives `.F.` at its limit
  (`tests/platform/no_threads`). *(Planned)*: a hierarchical timing wheel per worker, so that hundreds of thousands of
  pending timeouts cost constant time each.
- *(Planned)* A blocking pool, separate from the workers, runs work that has no asynchronous form (`getaddrinfo`, file
  I/O where the system has no asynchronous files, foreign libraries).

### Tasks

- A task object holds its state (created, scheduled, running, then completed, failed or canceled), its result or the
  value of its failure, its continuations, its block and the snapshot of its creator's environment until it runs; an
  `ASYNC` function's, its resume function, its frame and the point to resume at, and its parked environment (done).
- `TaskRun( bWork, [xArgs...] )` runs `Eval( bWork, xArgs... )` on the pool (`BASE/9024` for anything but a block or
  a function reference), in an environment made from a snapshot of its creator's at `TaskRun()`: the `SET` values,
  `ErrorBlock()`, the language and the code page, the default driver, and the PRIVATE variables visible (the same
  variables); work areas of its own, closed when it ends. What it changes in its own environment stays there.
- `oTask:Result` waits for the task and gives its result. A failure that carries an Error object is raised again in
  the waiter, through its error block (so `TRY`'s `CATCH` takes it, the default handler reports it); one that carries
  another value is raised as `Break( value )`; a cancellation is the error `BASE/9026` `Task canceled` (no generic
  code; the error block may substitute the result). `Wait( [nMs] )`
  waits (up to nMs milliseconds; a limit that is no number, `BASE/9024`) and tells whether the task ended; `IsCompleted`
  (ended, in any way), `IsFailed`, `IsCanceled`, `Error` (the failure's value, else NIL). A thread that waits for a
  task with no limit runs it itself when it has not started.
- An error in a task goes to its error block, its creator's; the default handler (`ErrorSys`) fails the task with the
  error instead of ending the program: a request that no `BEGIN SEQUENCE` of the task takes (a `RECOVER` meant for the
  code's own `BREAK`s does not swallow it), as outside a task it would end the program. So does an error no handler can
  take (no error block, a handler that returns for an error that cannot go on, handlers recursing too deep: the
  unrecoverable errors 9001 to 9003 elsewhere). A task run inline inside `Type()` has errors of its own, not `Type()`'s.
  A `BREAK` that leaves the block fails it with the `BREAK`'s value. `QUIT` ends the program, the task canceled; a wait
  for a task (or a `Mutex`, a `Condition`...) ends when the program is ending: with the `QUIT` that comes to its
  context, at once in code an `ALWAYS` runs meanwhile. At the program's end the tasks that never started are canceled.
- `oTask:ContinueWith( bWork )`: a task that runs `Eval( bWork, oTask )` once oTask ended (completed, failed or
  canceled), scheduled to run next on the worker that completed it (on the pool when another thread did).
  `TaskWhenAll( aTasks )`: a task that completes once all did, with their results in an array in their order; failed
  with the first failure in that order, canceled when one was; an empty array, completed with an empty one.
  `TaskWhenAny( aTasks )` (not empty): completed with the first task that ended. Both wait for the tasks the array held
  as one read saw it, and a chain of them of any length completes. Anything but an array of tasks is `BASE/9024`.
  `TaskFromResult( x )`: a task completed with x.
- `TaskDelay( nMs, [oToken] )`: a task that completes nMs milliseconds from now (0 or less: at once), canceled when
  the token is canceled first. `TaskTimeout( oTask, nMs )`: a task that ends as oTask does when it does within nMs
  milliseconds, else failed with the error `BASE/9027` `Task timed out`. `TaskYield()`: a task that completes (with
  `NIL`) as soon as anything waits for it (`AWAIT`, `Wait()`, `Result`, `ContinueWith()`, a combinator,
  `TaskTimeout()`), so `AWAIT TaskYield()` always suspends an `ASYNC` function, whose next step goes behind the work
  queued already (on its worker too), which a short `TaskDelay` does only when the delay has not passed yet.
- `CancellationTokenSource( [oToken...] )`: a source of cancellation, linked to the tokens given (canceled when one
  of them is): `Token`, `Cancel()` (once; again does nothing), `CancelAfter( nMs )`, `IsCancellationRequested`. A
  `CancellationToken`: `IsCancellationRequested`, `Register( bWork )` (a task that runs `Eval( bWork )` on the pool
  once the token is canceled), `ThrowIfCancellationRequested()` (the error `BASE/9028` `Operation canceled`,
  substitutable). Argument errors are `BASE/9024`.
- Timers fire in order of their deadlines, never before. A delay, a timeout or a `CancelAfter` still pending when the
  program ends is ended then: the delay and the timeout canceled, the source canceled (its `Register` tasks, scheduled
  then, are canceled with the program's other tasks that never started), so an `EXIT` procedure that waits for them is
  not held. A later `CancelAfter` replaces the time an earlier one set. `oSource:Dispose()` lets go
  of what the source is linked to and of its timer (`USING`): a long-lived token linked to again and again, as by
  each request of a server, keeps only the sources not disposed; a timeout settled before its deadline keeps nothing
  either (its timer is dropped once the heap grows).
- A task that fails while nothing has observed it (nothing waited for it: `Wait()`, `Result`, `AWAIT`; continued from
  it: `ContinueWith()`, `TaskWhenAll()`, `TaskWhenAny()`, `TaskTimeout()`, an `AWAIT` that suspends; read its failure:
  `Error`, `IsFailed`; `IsCompleted` and `IsCanceled` do not count; an asynchronous generator's body counts as observed:
  its enumerator takes its failure) is registered with the collector (rtl/tasks.c, the Task class's destructor). If it
  is still unobserved when the collector finds it unreachable, its failure goes to `__TaskUnobserved( xFailure, oTask )`
  (rtl/errorsys.prg): to the block `TaskUnobservedBlock( [bNew] )` set (a block replaces it, anything else is ignored,
  as `ErrorBlock()` does; one for the whole program), else to the standard error, an Error object as `Unobserved task
  failure: ` and the message and call lines the default handler would show, another value by its type. The program goes
  on. A task still reachable, completed or canceled is never reported; a fire-and-forget `ASYNC` call whose error fails
  it is (tests/run/task_unobserved).
- `TaskCompletionSource()`: `Task` is a task that its methods complete: `SetResult( x )`, `SetError( x )` (failed with
  x), `SetCanceled()`, each `.T.`, and `BASE/9025` `Task already completed` when it was; `TrySetResult`, `TrySetError`,
  `TrySetCanceled` tell whether they did.

### Asynchronous functions

Done (`compiler`: [compiler.md](compiler.md#syntax-tree-to-xir); runtime: `rtl/tasks.c`,
[runtime.md](runtime.md#asynchronous-functions)). An `ASYNC` function is compiled as a generator is: a resume function
(`NAME.resume`) whose frame is saved at each suspension and restored at the next step, which dispatches to the point
after that `AWAIT`; its own entry calls `xb_async_new`, which runs the first step at once. Nothing is allocated before
the first suspension: its task is made then (or, ended, at its end), with its frame (the variables live across the
`AWAIT`, an array), so a function that never suspends costs no frame save. `AWAIT` of a value that is no task, or of a
task that ended, goes on inline, with no allocation and no thread switch; one that suspends saves the frame, parks the
environment and registers the task as the awaited task's continuation: its next step is scheduled, like any task, when
that one ends (to run next on the worker that ended it). `AWAIT` of a call to one of the module's `ASYNC` functions,
or of a message (whose method may be one), takes the function's result itself when its first step does not suspend:
no task is made at all (the call site names the entry and its frame, and only that entry, called right from that
frame, answers: a native function or method in between never does). Measured (`bench/async.prg` against the same
program in C# with .NET's `Task`, `async`/`await` and `System.Threading.Channels`, on the same machine at the same
time): a million calls that complete at once, awaited, in about the time .NET takes (23-40 ms against 22-39 ms); a
hundred thousand round trips between two `ASYNC` functions over channels of one (four suspensions each) in about the
same as .NET (170-240 ms against 160-195 ms); ten thousand handlers suspended at once on delays, about 25 ms against
20. The Basel sum over 32 `ASYNC` functions awaiting CPU-bound `TaskRun`s (`bench/async_pi.prg`, 1,600,000,000
terms) spreads over every processor; .NET's loop is about 1.4 times faster there, for reasons of numeric code, not of
the tasks.

### Channels

Done (`rtl/channels.c`). `Channel( [nCapacity] )` keeps up to nCapacity values
sent and not received yet (without it, or 0: any number; a capacity below 0, past 2^31 - 1 or no number,
`BASE/9029`).

- `SendAsync( xValue, [oToken] )`: a task that completes (`.T.`) once the channel took the value: at once when a
  receiver waits (it gets it) or there is room; else when a receiver makes room, in the order senders came; the token
  cancels the wait (the value is not kept then). `ReceiveAsync( [oToken] )`: a task that completes with the next value:
  at once when there is one; else when a sender gives one, receivers served in the order they came; the token cancels
  the wait, and a value goes only to a receiver that takes it. A token that is no token is `BASE/9024`.
- `TrySend( xValue )` and `TryReceive( [aSlot] )` never wait: whether the value was taken, or one was received (into
  `aSlot[ 1 ]`). `Count()`: the values kept; `IsClosed`.
- `Close()`: nothing is sent any more: waiting senders and receivers fail with `BASE/9030` `Channel closed`, the values
  kept are still received, then `ReceiveAsync` fails so too; closing again does nothing.
- One sender's values arrive in the order it sent them. `FOR EACH x IN oChannel` takes the values as they come, waiting
  for each, until the channel is closed and empty (an enumerator of its own for each `FOR EACH`, in C: the hot path of a
  consumer). The enumerator's `MoveNextAsync( [oToken] )` awaits instead of holding the thread: a task of whether a
  value came (`.F.` once the channel is closed and empty, also when it closes while the task waits), the value in
  `Current`; its waits are served with `ReceiveAsync`'s, in the order they came. `GetEnumerator( [oToken] )` gives the
  token that cancels the waits of the `MoveNextAsync()` calls given none of their own (something that is no token is
  `BASE/9024`, as for `MoveNextAsync`). One enumerator waits for one value at a time: a `MoveNextAsync()` asked while
  another of the same enumerator waits raises `BASE/9035` `Enumerator waiting already` (both values would go into one
  `Current`).
- Cost (measured): a `TrySend` and `TryReceive` pair about 40 ns, a `SendAsync` and `ReceiveAsync` pair that waits for
  nothing about 90 ns; a value handed between tasks that block waiting for each other costs a thread's wake (about a
  microsecond), which `AWAIT` (phase 4) removes: a waiting task suspends instead of blocking its thread.
- A receiver or a sender that a token canceled gives its place up: the channel drops such waiters when its ring of
  them is full, and grows the ring all the same when that freed less than half of it, so waits canceled again and
  again cost what they are (however many waiters stay) and keep nothing on a quiet channel. A ring shrinks to half
  once a quarter of it is used: a burst's room is given back.
- The channel's state is its own object's, under its lock (a word: one compare-and-swap to take it, one exchange to
  release it, loads before a waiter sleeps on it, a short wait during which collections go on): rings over arrays for
  the values, the receivers waiting and the senders waiting, read and written in place by the lock's holder. The lock is
  held for the rings alone: a waiter's task is made before it is taken; a waiter the other side serves is claimed under
  it (`xb_task_claim`: its token cancels it no more; a cancellation asked meanwhile is kept, so that a sender that gives
  its claim back to wait in line is canceled instead) and completed, with its wake and continuations, once it is
  released, as are the waiters that `Close()` fails. A send that waits for nothing returns one task completed with `.T.`
  that all such sends share, so it allocates nothing.

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

- **Local and shared.** An object is *local* while only one flow can reach it, and *shared* once two can (a bit of the
  header's state byte, `XB_GC_SHARED`). Sharing is transitive: everything a shared object refers to is shared.
- **Publication.** A reference becomes reachable by another flow only at known places, and each one shares the object
  and what it reaches (once per object, like marking): a store into a shared object or into shared storage (module
  `STATIC`s, `PUBLIC`s, class variables, a channel, a task's result), the arguments and captured cells of a new flow
  (`TaskRun`, `ThreadStart`), and the frame of an `ASYNC` call that suspends while its caller goes on without awaiting
  it. The write barrier, which every store into a heap object already passes through, tests the stored-into object's
  header byte; the shared bit lives in the same byte, so local stores cost nothing more. The program's storage (the
  `STATIC`s, the class variables, the memory variables) becomes shared storage when a second thread attaches
  (`xb_gc_thread_attach`): every other thread stopped, as for a collection, what it holds is shared (the memory
  variables visible then: shared
environments, and the variables made in them, come with `ThreadStart`) and the flag
  `xb_storage_shared` set ([runtime.md](runtime.md#threads-and-synchronization)). From then on a `STATIC`'s own item is
  read and written whole under the stripe of its address, a value stored shared first (compiled code tests the flag and
  calls the runtime); a `STATIC` moved into a cell (captured, passed by reference, a `FOR EACH` variable) is moved under
  that stripe, the cell shared first; a class variable is read and written so too, and a class prepared later shares
  what it holds before another thread can reach it. An attached thread that waits for another (a join, a wait for a
  flag) waits in a blocking operation, as for a collection.
- **Access.** A shared object's items are read under its sequence counter (read, check, retry: no lock, no write) and
  written, grown and shrunk under its lock: those of its stripe, one of a table chosen by its address, whose lock word a
  contended waiter sleeps on in the parking lot. Compiled code's in-place reads and writes (an array's elements and
  length, a hash's keys in `FOR EACH`, a cell's value, a loop's reference, an object's instance variable at a send) test
  the header bit first (one byte loaded and a branch, predicted; a fast loop that makes no call tests it once, before
  it, for an array it does not change: [compiler.md](compiler.md#object-emission)) and call the runtime when it is set
  ([compiler.md](compiler.md#xir-to-llvm)). A shared array holds items: one of a compact form (bytes or words of
  logicals and integers, [runtime.md](runtime.md#values)) turns into items in place as it is shared, before another flow
  can reach it (the arrays of the program's storage at its publication, while the other threads are stopped), so only
  the flow that reaches a local array changes its form while that flow runs. Strings are immutable; a shared string
  never grows in place (its append flag is cleared when it is shared). Flows may append to one shared view at once: each
  claims the bytes past its buffer's tip with an atomic compare-and-swap, and one that loses copies
  ([runtime.md](runtime.md#values)). A shared hash is looked up by key, and its length read, under its sequence counter:
  the store's addresses and the index as one read sees them, then a probe that copies each key it compares and checks
  the read before it looks into the key, with a bounded number of slots; a writer that came between makes it read again,
  and after eight reads it looks up under the lock, which hands itself over to a waiter writers keep overtaking (a
  lookup never misses a key the hash held throughout: tests/platform/shared_hashes). A writer's count goes odd before
  anything it writes, plain stores too (a release fence after it, for weak-memory targets). Four threads that look one
  shared hash up take a sixth of the cycles they took under its lock. Read-only operations that reorganize a hash
  (positions, through the position tree) run under the lock when it is shared.
- **Lifetime.** A collection happens only at safepoints, and no code holds the address of a shared object's storage
  across one: a read under the sequence counter has no safepoint inside, compiled code tests the header bit at each
  access and reads the storage's address again after every call, and the runtime's loops that run code between accesses
  (a sort with a block) index the object again after each call. So storage another thread replaced (an array that grew)
  dies at the next collection like any garbage, never under a reader. (Keeping alive what the stacks point to, which a
  conservative scan could do, was measured: a stale word on a stack kept long chains of dead objects alive, and
  destructors no longer ran when their objects died; the collector stays precise, and its stack scan only pins.)
- **Implementation steps.** (1, done: [runtime.md](runtime.md#threads-and-synchronization)) The header bit
  (`XB_GC_SHARED` in the state byte), sharing an object graph (an explicit stack, stopping at what is shared already; a
  string loses its append flag), and the sequence counters and locks: a table of stripes chosen by the object's address
  (an object moves only while every thread is stopped, between operations), each a counter (odd while a writer works)
  and a lock word parked on when contended; with C tests on threads. (2, done) The runtime's operations on arrays,
  hashes, object variables, cells, references, the elements `FOR EACH` reads and generators take the shared path when
  the bit is set. (3, done) Compiled code's in-place reads and writes test the bit, measured on the benchmarks (ABI 44).
  (4, done) Publication: a store into a shared object shares the value stored first; `STATIC`s, class variables and
  memory variables are shared storage once a second thread exists (ABI 45). (2e, done) `ThreadStart`, `Mutex`,
  `AtomicInt`, the program-wide `PUBLIC` table and line-atomic console output; then `Condition`, `Semaphore`, `USING`
  and `SYNCHRONIZED` ([Threads](#threads)).
- **Runtime caches and tables** filled at run time (done, [runtime.md](runtime.md#threads-and-synchronization)): a
  send cache is a sequence lock, a field cache one 64-bit word (generation, index) from an atomic counter, the memory
  variable names are found under a reader-writer lock and read by number from chunks that never move, and classes
  are prepared under one recursive lock, a ready class published atomically.

## Garbage collector

The collector ([runtime.md](runtime.md#garbage-collector)) gets, in this order:

1. **Stop the world** (done). A collection asks every other running thread to stop (a bit of `pending` on its context,
   set atomically) and waits for each one to reach a safepoint: compiled code tests `pending` after calls and in loops
   (on each pass of a loop, or once every 1024 passes of an innermost loop without calls), the macro evaluator after
   each operation that can fail, allocations on their slow paths; runtime loops that evaluate blocks reach those polls
   through them, and only loops of C alone (a sort without a block) hold a collection up for as long as they run. A
   thread in a blocking operation is already stopped: the operation runs with the thread's registers saved on its stack,
   its stack's top published, and the thread does not touch the heap until it has checked for a collection on its way
   back. Waiting for a runtime lock under which xBase code may run (the lock of class preparations, whose holder
   evaluates INIT values and error blocks) is such a blocking operation, or a collection the holder starts would wait
   for the waiter forever.
2. **Every stack** (done). The scan for pinning covers every thread's stack, up to the end the system reports.
   Suspended tasks have no stack: their frames are heap objects, traced precisely.
3. **Thread-safe mutators** (done). Header state bits a mutator sets change with atomic byte operations; the remembered
   set is filled through per-thread lists merged at collection, not under the heap lock; destructors run on one
   context at a time, the program's, or the allocating thread's while the program's thread waits in a blocking
   operation (*planned*: on a finalizer task of their own).
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
2. **A thread-safe runtime**: the runtime caches and global state made thread-safe (done), the context split and
   environments with the stack error (done); stop-the-world collections with back-edge polls (ABI), every stack scanned,
   atomic headers, per-thread remembered sets; shared objects (header bit, publication, sequence
   counters and locks, compiled fast paths); line-atomic console output, `ThreadStart`, `Mutex`, `AtomicInt`,
   `Condition`, `Semaphore`, `SYNCHRONIZED` (done). The first visible feature: threads sharing memory safely.
3. **Scheduler and tasks**: workers, run queues, stealing, parking, blocking handoff, `//WORKERS:n`, `Task`,
   `TaskRun`, `TaskWhenAll`/`Any`, `TaskFromResult`, `TaskCompletionSource`, a start procedure's task waited for
   (done, step 3a); the handoff's monitor, timers, `TaskDelay`, `TaskTimeout`, cancellation (done, step 3b); `Channel`
   (done, step 3c); usable from synchronous code (`Wait()`, `Result`, `ContinueWith`). The blocking pool comes with the
   asynchronous I/O that needs it (phase 5).
4. **The compiler**: `ASYNC` functions, procedures and methods, `AWAIT` everywhere in them, the start procedure
   awaiting, errors and cancellation across `AWAIT`, the completed fast path and fused calls (done, step 4a);
   `AWAIT FOR EACH` and asynchronous generators (done); asynchronous codeblocks; `AWAIT USING` (`USING` is done).
5. **Asynchronous I/O**: the reactors (io_uring and epoll, kqueue, IOCP), sockets, asynchronous files, name resolution
   on the blocking pool; the echo server benchmark.
6. **The database**: awaitable operations on the blocking pool for every driver, lock waits with timeouts and tokens,
   cursors as asynchronous enumerators, the `AWAIT` commands; then the shared page cache, read-ahead and the inline
   fast path; SQLite (RDD step 7).
7. **Libraries**: HTTP server and client, WebSocket, TLS, JSON, database clients; the HTTP benchmark with 100,000
   connections.
8. **The collector at scale**: parallel marking and lazy sweeping, then concurrent marking and arenas, against the
   pause and memory targets of phase 7's benchmarks.
