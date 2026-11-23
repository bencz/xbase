// FOR with an integer counter and step and a limit that is a double (a remainder, a quotient, a literal with decimals,
// an infinity: Log( -1 )), up and down, compared as numbers are: the counter as a double, so past 2^53 it rounds as the
// limit does; a NaN limit ends the loop at once; a limit that is no number is the comparison's error. The same loops
// with the limit known and unknown to the compiler (a parameter).
PROCEDURE Main()
   LOCAL oErr
   ? "remainder:", Up( 10 - 10 % 3 ), Up( 7 % 4 + 0.5 )
   ? "quotient:", Up( 10 / 4 ), Down( 10 / 4 ), Down( -1.5 )
   ? "equal:", Up( 9 / 3 ), Down( 6 / 3 ), Up( 1 / 1 ), Down( 3 / 1 )
   ? "literals:", Literals()
   ? "infinite:", Up( Log( -1 ) ), Down( -Log( -1 ) )
   ? "nan:", Up( Log( -1 ) - Log( -1 ) ), Down( Log( -1 ) - Log( -1 ) )
   ? "large:", Large( 9007199254740993 ), Large( 9007199254740992.0 )
   ErrorBlock( {| e | Break( e ) } )
   BEGIN SEQUENCE
      ? Up( "x" )
   RECOVER USING oErr
      ? "not a number:", oErr:subCode, oErr:operation
   END SEQUENCE
RETURN

// How many passes, and the counter after the loop.
STATIC FUNCTION Up( nLimit )
   LOCAL i, n := 0
   FOR i := 1 TO nLimit
      n++
   NEXT
RETURN LTrim( Str( n ) ) + "/" + LTrim( Str( i ) )

STATIC FUNCTION Down( nLimit )
   LOCAL i, n := 0
   FOR i := 3 TO nLimit STEP -1
      n++
   NEXT
RETURN LTrim( Str( n ) ) + "/" + LTrim( Str( i ) )

STATIC FUNCTION Literals()
   LOCAL i, n := 0, m := 0
   FOR i := 1 TO 4.5
      n++
   NEXT
   FOR i := 5 TO 0.5 STEP -2
      m++
   NEXT
RETURN LTrim( Str( n ) ) + " " + LTrim( Str( m ) )

// A counter from 2^53 - 1 by 1 to a limit near 2^53: compared as doubles, 2^53 + 1 is 2^53.
STATIC FUNCTION Large( nLimit )
   LOCAL i, n := 0
   FOR i := 9007199254740991 TO nLimit
      n++
      IF n > 5
         EXIT
      ENDIF
   NEXT
RETURN LTrim( Str( n ) )
