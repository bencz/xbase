// Loop nests whose fast copies leave for their generic copy (a slow path in an inner loop: an integer that overflows, a
// string, a double, an index out of the array) and come back, inner loops of several latches (LOOP), EXIT and RETURN
// from inside a nest, values computed in the loops used after them, and loops whose invariant tests (an array's type
// and bounds) are made once before them: the same function given an array, then a hash, then an array too short; and
// a product of matrices whose inner loop reads, on entry, the sharing state of a matrix it indexes by its counter.
PROCEDURE Main()
   LOCAL a := Grid(), b, n, c := 0, e
   ? "sums:", Sums( a )
   ? "latches:", Latches( 30 )
   ? "exit:", FindFirst( a, "x" ), FindFirst( a, 7 ), FindFirst( a, -1 )
   b := { 10, 20, 30, 40 }
   ? "array:", Total( b, 4 ), "hash:", Total( { 1 => 1.5, 2 => 2, 3 => 3, 4 => 4 }, 4 )
   ErrorBlock( {| x | Break( x ) } )
   BEGIN SEQUENCE
      ? "short:", Total( b, 6 )
   RECOVER USING e
      ? "short: error", e:subCode
   END SEQUENCE
   FOR n := 1 TO 3
      c += Total( b, 4 ) // the versioned loop again, after its exact copy ran
   NEXT
   ? "again:", c
   ? "deep:", Deep( 6 )
   ? "product:", Product( 7 )
RETURN

// Rows of numbers: integers, a double, a string, and integers large enough that their sum overflows to a double.
STATIC FUNCTION Grid()
   LOCAL a := {}, i, j, row
   FOR i := 1 TO 8
      row := {}
      FOR j := 1 TO 9
         AAdd( row, iif( i == 3 .AND. j == 4, 2.5, ;
                    iif( i == 5 .AND. j == 2, "x", iif( i == 7, 0x3FFFFFFFFFFFFFFF, i * j ) ) ) )
      NEXT
      AAdd( a, row )
   NEXT
RETURN a

STATIC FUNCTION Sums( a )
   LOCAL i, j, k, s := 0, t := 0, cs := "", v
   FOR k := 1 TO 3
      FOR i := 1 TO Len( a )
         FOR j := 1 TO Len( a[ i ] )
            v := a[ i, j ]
            IF ValType( v ) == "C"
               cs += v
            ELSEIF i == 7
               t += v // overflows past 2^63 on the second element: a double from then on
            ELSE
               s += v
            ENDIF
         NEXT
      NEXT
   NEXT
RETURN LTrim( Str( s ) ) + " " + LTrim( Str( t, 30, 0 ) ) + " " + cs + " " + LTrim( Str( i ) ) + " " + LTrim( Str( j ) )

// Inner loops left early by LOOP (several back edges) and by EXIT.
STATIC FUNCTION Latches( n )
   LOCAL i, j, s := 0
   FOR i := 1 TO n
      j := 0
      DO WHILE j < i
         j++
         IF j % 3 == 0
            LOOP
         ELSEIF j % 7 == 0
            s += 100
            LOOP
         ELSEIF j > 20
            EXIT
         ENDIF
         s += j
      ENDDO
      IF i % 5 == 0
         LOOP
      ENDIF
      s += i
   NEXT
RETURN s

// RETURN from the innermost loop of a nest.
STATIC FUNCTION FindFirst( a, x )
   LOCAL i, j
   FOR i := 1 TO Len( a )
      FOR j := 1 TO Len( a[ i ] )
         IF ValType( a[ i, j ] ) == ValType( x ) .AND. a[ i, j ] == x
            RETURN LTrim( Str( i ) ) + "," + LTrim( Str( j ) )
         ENDIF
      NEXT
   NEXT
RETURN "none"

// A loop over `c`, which it does not change: its type tested once before it, though the loop makes another test first
// (the sum of the counters, which could overflow).
STATIC FUNCTION Total( c, n )
   LOCAL k, s := 0, t := 0
   FOR k := 1 TO n
      t += k
      s += c[ k ]
   NEXT
RETURN s + t - Int( n * ( n + 1 ) / 2 )

// Six nested loops, the innermost running fast, each level adding what the inner ones made.
STATIC FUNCTION Deep( n )
   LOCAL a, b, c, d, e, f, s := 0, x := { 1, 2, 3, 4, 5, 6 }
   FOR a := 1 TO n
      FOR b := 1 TO a
         FOR c := 1 TO b
            FOR d := 1 TO c
               FOR e := 1 TO d
                  FOR f := 1 TO e
                     s += x[ f ] * x[ e ] - x[ d ]
                  NEXT
                  s += e
               NEXT
            NEXT
            s -= c
         NEXT
      NEXT
   NEXT
RETURN s

STATIC FUNCTION Product( n )
   LOCAL a := Array( n, n ), b := Array( n, n ), i, j, k, s, total := 0
   FOR i := 1 TO n
      FOR j := 1 TO n
         a[ i, j ] := i + j
         b[ i, j ] := i * j % 5
      NEXT
   NEXT
   FOR i := 1 TO n
      FOR j := 1 TO n
         s := 0
         FOR k := 1 TO n
            s := s + a[ i, k ] * b[ k, j ]
         NEXT
         total += s * ( i + j )
      NEXT
   NEXT
RETURN total
