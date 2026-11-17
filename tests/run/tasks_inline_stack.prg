// A chain of waits, each task waiting with :Result for the next one it starts (which a waiter runs inline, on its own
// stack), 20000 deep: from the program's thread and from a worker, the waiter whose stack runs short stops running
// them inline and waits, so a worker goes on with the chain on a stack of its own; no stack error, every level counted.
// (A recursion too deep for any stack still raises the stack error: stack_overflow.prg.)
PROCEDURE Main()
   ? "from the program:", Chain( 20000 )
   ? "from a worker:", TaskRun( {|| Chain( 20000 ) } ):Result
RETURN

STATIC FUNCTION Chain( n )
   IF n == 0
      RETURN 0
   ENDIF
RETURN TaskRun( {|| Chain( n - 1 ) } ):Result + 1
