// A value that enters a loop where a number is expected is checked once where it is made (a guard); a call's result
// that is no number (here a large string, the FOR's limit) leaves the typed loop for a generic one, and it must stay
// alive until then: another thread collects all the time, and the FOR's error (BASE/1075), whose block substitutes .T.,
// comes after the check. Thousands of passes: none loses the string (a lost one would crash the collector or show a
// wrong length).
STATIC s_lStop := .F.

PROCEDURE Main()
   LOCAL t, k, n := 0, nLen := 0
   t := ThreadStart( {|| Collector() } )
   ErrorBlock( {| e | iif( e:SubCode == 1075, .T., Break( e ) ) } ) // the FOR's test, i > nTo: .T. ends the loop
   FOR k := 1 TO 3000
      n += Loop( k )
      nLen += Len( Limit( k ) )
   NEXT
   s_lStop := .T.
   t:Join()
   ? "passes:", n, "lengths:", nLen
RETURN

STATIC FUNCTION Loop( k )
   LOCAL i, nTo := Limit( k, 1 ), n := 0
   FOR i := 1 TO nTo
      n++
   NEXT
RETURN n

// A compiled function's call (recursive: never inlined), whose pending test right after it may collect.
FUNCTION Limit( k, nDepth )
   IF nDepth != NIL .AND. nDepth > 0
      RETURN Limit( k, nDepth - 1 )
   ENDIF
RETURN Replicate( Chr( 65 + k % 26 ), 200000 + k % 7 )

STATIC FUNCTION Collector()
   LOCAL x, a
   DO WHILE !s_lStop
      FOR x := 1 TO 20
         a := Array( 10 )
      NEXT
      __GcCollect()
   ENDDO
RETURN NIL
