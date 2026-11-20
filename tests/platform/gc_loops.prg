// Loops without calls, for platform.gc_loops. Four run until the program ends and reach a safepoint only through
// their polls: a counted loop (its exit test compares with a stop), one whose end depends on its data (it counts its
// passes down), one whose variable steps over its end (`ne` by 2 towards an odd distance: it never meets it, and
// keeps its stops), and one whose calls take a leaf's early return in place (the callee's small entry, inlined: the
// test after the call is the loop's only one, kept though nothing was called since the last pass's). Sums() runs such
// loops to their end: its result must not change when polls serve collections.
PROCEDURE SpinCounted()
   LOCAL i, n := 0, nTo := 4000000000000000
   FOR i := 1 TO nTo
      n := ( n + i ) % 1000
   NEXT
RETURN

PROCEDURE SpinWhile()
   LOCAL n := 0
   DO WHILE n != -1
      n := ( n + 7 ) % 1000
   ENDDO
RETURN

PROCEDURE SpinPast()
   LOCAL i, nEnd := 13
   FOR i := 0 TO 10 STEP 2 // i is known to be an integer after it
   NEXT
   DO WHILE ! ( i == nEnd )
      i := i + 2
   ENDDO
RETURN

PROCEDURE SpinLeaves()
   LOCAL i := 0, n := 0
   DO WHILE n != -1
      n := ( n + Leaf( i % 2 ) ) % 1000
      i++
   ENDDO
RETURN

STATIC FUNCTION Leaf( n )
   IF n < 2
      RETURN n
   ENDIF
RETURN Leaf( n - 1 ) + Leaf( n - 2 )

FUNCTION Sums()
   LOCAL i, n := 0, nTo := 300000, m := 27, nSteps := 0
   FOR i := 1 TO nTo
      n := n + i % 13
   NEXT
   FOR i := nTo TO 1 STEP -3
      n := n + i % 7
   NEXT
   DO WHILE m != 1
      m := IIf( m % 2 == 0, Int( m / 2 ), 3 * m + 1 )
      nSteps++
   ENDDO
RETURN n * 1000 + nSteps
