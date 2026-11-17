// The Basel sum by ASYNC functions (the bench/async_pi.prg program at a small size): each of the tasks sums 1/n^2 over
// its own range of terms on a worker (an ASYNC function awaiting TaskRun), the start procedure awaits TaskWhenAll and
// adds the partial sums in order; pi is Sqrt( 6 * sum ). The digits are exact: each partial sum, and their total,
// adds in one order.
#define TASKS 32
#define TERMS 2000

PROCEDURE Main()
   LOCAL a := {}, k, x, nTotal := 0, nPi
   FOR k := 0 TO TASKS - 1
      AAdd( a, Partial( k, TERMS ) )
   NEXT
   FOR EACH x IN AWAIT TaskWhenAll( a )
      nTotal += x
   NEXT
   nPi := Sqrt( nTotal * 6 )
   ? "approximate pi:", Str( nPi, 16, 12 )
   ? "real pi:       ", Str( 3.141592653589793, 16, 12 )
   ? "difference:    ", Str( 3.141592653589793 - nPi, 16, 12 )
   ? "tasks:", Len( a ), "terms:", TASKS * TERMS
RETURN

ASYNC FUNCTION Partial( i, nTerms )
RETURN AWAIT TaskRun( {|| Basel( i * nTerms + 1, ( i + 1 ) * nTerms ) } )

STATIC FUNCTION Basel( nFrom, nTo )
   LOCAL n, nInv, nSum := 0
   FOR n := nFrom TO nTo
      nInv := 1 / n
      nSum += nInv * nInv
   NEXT
RETURN nSum
