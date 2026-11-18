// An asynchronous generator whose body waits on a task when the program's start procedure returns: the program's
// end cancels that task and the body's (queued, never run again), and the task of the MoveNextAsync() that waited is
// canceled with them: an EXIT procedure finds it canceled, and the enumeration over (.F., not "waiting already").
STATIC s_g, s_t
PROCEDURE Main()
   s_g := Slow()
   s_t := s_g:MoveNextAsync()
   ? "main returns, waiting:", !s_t:IsCompleted
RETURN

EXIT PROCEDURE After()
   LOCAL e
   ? "at the end: canceled", s_t:IsCanceled, "completed", s_t:IsCompleted
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      ? "then:", s_g:MoveNextAsync():Result
   RECOVER USING e
      ? "then:", e:description
   END SEQUENCE
RETURN

ASYNC FUNCTION Slow()
   AWAIT TaskRun( {|| Busy() } )
   YIELD 1
RETURN

STATIC FUNCTION Busy()
   LOCAL n := ElapsedMs()
   DO WHILE ElapsedMs() - n < 2000
   ENDDO
RETURN NIL
