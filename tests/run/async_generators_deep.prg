// An asynchronous generator consumed where the stack is short: on a thread, how deep a recursion goes before the stack
// error is measured first; then, 300 levels above that, a generator made there (local to the thread) is stepped by
// hand: its steps run on a worker (no room to run them on this stack), the elements come in order, and the end comes.
PROCEDURE Main()
   LOCAL a := {}
   ThreadStart( {|| AAdd( a, Consume() ) } ):Join()
   ? "deep consumer:", a[ 1 ]
RETURN

STATIC FUNCTION Consume()
   LOCAL nMax := 0, e
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      Walk( 1, 0, @nMax )
   RECOVER USING e
   END SEQUENCE
RETURN Walk( 1, nMax - 300 )

// One function for both (its frame the same): with nAt 0, how deep it goes (nMax); else down to nAt, then the steps.
STATIC FUNCTION Walk( n, nAt, nMax )
   LOCAL g, c := ""
   IF nAt == 0
      nMax := n
      RETURN Walk( n + 1, 0, @nMax ) + 1
   ENDIF
   IF n < nAt
      RETURN Walk( n + 1, nAt )
   ENDIF
   g := Items( 3 )
   DO WHILE g:MoveNextAsync():Result
      c += LTrim( Str( g:Current ) ) + " "
   ENDDO
RETURN c + "end"

ASYNC FUNCTION Items( n )
   LOCAL k
   FOR k := 1 TO n
      AWAIT TaskYield()
      YIELD k
   NEXT
RETURN
