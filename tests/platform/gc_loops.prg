// Loops without calls, for platform.gc_loops. Three run until the program ends and reach a safepoint only through
// their polls: a counted loop (its exit test compares with a stop), one whose end depends on its data (it counts its
// passes down), and one whose variable steps over its end (`ne` by 2 towards an odd distance: it never meets it, and
// keeps its stops). Sums() runs such loops to their end: its result must not change when polls serve collections.
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
