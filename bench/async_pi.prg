// The Basel sum by 32 ASYNC functions (a port of a C# async/await program): each sums 1/n^2 over its own 50,000,000
// terms on a worker (an ASYNC function awaiting TaskRun), the start procedure awaits TaskWhenAll and adds the partial
// sums; pi is Sqrt( 6 * sum ). What CPU-bound tasks spread over every processor gain, through async/await.
#define TASKS 32
#define TERMS 50000000

PROCEDURE Main()
   LOCAL t := ElapsedMs(), a := {}, k, x, nTotal := 0, nPi
   FOR k := 0 TO TASKS - 1
      AAdd( a, Partial( k, TERMS ) )
   NEXT
   FOR EACH x IN AWAIT TaskWhenAll( a )
      nTotal += x
   NEXT
   nPi := Sqrt( nTotal * 6 )
   ? "async_pi", ElapsedMs() - t, IIf( Abs( 3.141592653589793 - nPi ) < 0.000000001, "ok", "WRONG" )
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
