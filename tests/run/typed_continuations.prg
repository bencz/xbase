// A value the error block substitutes for a result inside a loop, of a type
// the operation never gives itself, continues the loop correctly (the typed
// code leaves for a generic copy of the rest of the function).
PROCEDURE Main()
   LOCAL i, x, d, s, n, a := {}
   ErrorBlock( {| e | Handler( e ) } )
   x := 0
   FOR i := 1 TO 5
      d := 100 / ( i - 3 )      // a zero divisor at i = 3 substitutes a string
      x := x + Len( Str( d ) )
      ? i, d, x
   NEXT
   s := 0
   n := 1.5
   DO WHILE n < 40
      n := n * 2 - ( n / ( n - 6 ) ) // a zero divisor substitutes a date
      s := s + 1
      ? s, n, ValType( n )
      IF ValType( n ) == "D"
         EXIT
      ENDIF
   ENDDO
   FOR i := 1 TO 4
      d := -( i % ( i - 2 ) )   // a substituted array, then the unary minus substitutes a logical
      AAdd( a, d )
   NEXT
   ? Len( a ), ValType( a[ 1 ] ), ValType( a[ 2 ] ), a[ 3 ], a[ 4 ]
   ? Sum( 6 ), Sum( 0 )
   s := ""
   FOR i := 1 TO 5
      s := s + IIf( i == 3, 5, "x" )   // a string, until the error of "xx" + 5 substitutes a number
      ? i, s
   NEXT
RETURN

STATIC FUNCTION Handler( e )
   SWITCH e:subCode
   CASE 1340
      RETURN IIf( e:operation == "/", IIf( e:args[ 1 ] == 100, "big", SToD( "20240102" ) ), 0 )
   CASE 1341
      RETURN { 7 }
   CASE 1080
      RETURN .T.
   CASE 1081
      RETURN Len( e:args ) * 100
   ENDSWITCH
RETURN 0

STATIC FUNCTION Sum( n )
   LOCAL i, t := 0
   FOR i := 1 TO 3
      t := t + 12 / n
   NEXT
RETURN t
