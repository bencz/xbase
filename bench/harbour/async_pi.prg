// bench/async_pi.prg in Harbour: the Basel sum over 32 threads of 50,000,000 terms each (its threads where xbase has
// ASYNC functions and tasks), joined in order; pi is Sqrt( 6 * sum ).
#define TASKS 32
#define TERMS 50000000

PROCEDURE Main()
   LOCAL t := ElapsedMs(), a := {}, k, x, nPart, nTotal := 0, nPi
   FOR k := 0 TO TASKS - 1
      AAdd( a, hb_threadStart( @Basel(), k * TERMS + 1, ( k + 1 ) * TERMS ) )
   NEXT
   FOR EACH x IN a
      hb_threadJoin( x, @nPart )
      nTotal += nPart
   NEXT
   nPi := Sqrt( nTotal * 6 )
   ? "async_pi", ElapsedMs() - t, IIf( Abs( 3.141592653589793 - nPi ) < 0.000000001, "ok", "WRONG" )
RETURN

STATIC FUNCTION Basel( nFrom, nTo )
   LOCAL n, nInv, nSum := 0
   FOR n := nFrom TO nTo
      nInv := 1 / n
      nSum += nInv * nInv
   NEXT
RETURN nSum
