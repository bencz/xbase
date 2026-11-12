// Loops polled once every so many passes (codegen.loop_strip_mined, codegen.loop_count_down): counted loops, whose
// exit test compares with a stop until the real end comes, going up and down, by steps of 1 and more, across many
// stops, ending at a stop or between two, left by EXIT and with LOOP; and a loop whose end depends on its data, which
// counts its passes down. Every result is the loop's own.
PROCEDURE Main()
   ? Up( 1, 5000 ), Up( 1, 2048 ), Up( 7, 7 ), Up( 9, 1 ), Up( -3000, 3000 )
   ? Down( 5000, 1 ), Down( 1, 1 ), Down( 1, 2 )
   ? Steps( 1, 10000, 3 ), Steps( 10000, 1, -7 ), Steps( 0, 1, 1000 )
   ? Leave( 100000, 4097 ), Again( 6000 )
   ? Nested( 70, 90 ), Collatz( 27 ), Collatz( 837799 ), ByRef()
RETURN

// A bound passed by reference, which the loop reads again on every pass.
FUNCTION ByRef()
   LOCAL n := 3000
RETURN Up2( @n )

FUNCTION Up2( nTo )
   LOCAL i, n := 0
   FOR i := 1 TO nTo
      n += i
   NEXT
RETURN n

// The bounds in LOCALs: parameters may be references, which a FOR reads again on every pass.
FUNCTION Up( nFrom, nTo )
   LOCAL i, n := 0, nA := nFrom, nB := nTo
   FOR i := nA TO nB
      n += i
   NEXT
RETURN n

FUNCTION Down( nFrom, nTo )
   LOCAL i, n := 0, nA := nFrom, nB := nTo
   FOR i := nA TO nB STEP -1
      n += i % 7
   NEXT
RETURN n

FUNCTION Steps( nFrom, nTo, nStep )
   LOCAL i, n := 0, nA := nFrom, nB := nTo, nBy := nStep
   FOR i := nA TO nB STEP nBy
      n += i
   NEXT
RETURN Str( n ) + "/" + LTrim( Str( i ) )

FUNCTION Leave( nTo, nAt )
   LOCAL i, n := 0, nB := nTo
   FOR i := 1 TO nB
      IF i == nAt
         EXIT
      ENDIF
      n += i
   NEXT
RETURN Str( n ) + "/" + LTrim( Str( i ) )

FUNCTION Again( nTo )
   LOCAL i, n := 0, nB := nTo
   FOR i := 1 TO nB
      IF i % 3 == 0
         LOOP
      ENDIF
      n += i
   NEXT
RETURN n

FUNCTION Nested( nRows, nCols )
   LOCAL i, j, n := 0, nR := nRows, nC := nCols
   FOR i := 1 TO nR
      FOR j := i TO nC
         n += i * j
      NEXT
   NEXT
RETURN n

// The steps of the Collatz sequence from n to 1: a loop whose end depends on its data.
FUNCTION Collatz( n )
   LOCAL nSteps := 0
   DO WHILE n != 1
      n := IIf( n % 2 == 0, Int( n / 2 ), 3 * n + 1 )
      nSteps++
   ENDDO
RETURN nSteps
