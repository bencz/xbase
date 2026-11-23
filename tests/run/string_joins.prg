// A chain of `+` whose pieces may be strings is joined in one string, each `+` still in its place: the pieces are
// computed in order, and a `+` whose piece is no string fails right there, before the pieces after it are computed (its
// error's arguments: the sum so far and that piece); a value the error's handler substitutes is the sum the chain goes
// on from, `+` by `+`. An object's class `+` receives the next piece once. Pieces empty, of one byte and long; a chain
// in an inlined function whose sum is a piece of another chain; chains whose pieces' types are known or not; through
// a macro alike; a last piece that is no string (its error, or a substituted value); a date among the pieces; a loop
// whose pieces turn from strings into a number (the generic copy takes over mid-chain); a variable that is a piece
// of the chain assigned its sum; and a chain longer than one join (kMaxJoinPieces), called recursively.
STATIC s_cLog := ""

PROCEDURE Main()
   LOCAL oErr, c, i, cLong := Replicate( "x", 600 )
   ? "strings:", Join3( "ab", "-", "cd" ), Join5( "a", "b", "c", "d", "e" ), Len( Join3( cLong, "|", cLong ) )
   ? "empty and one byte:", "[" + Join3( "", "", "" ) + "]", Join3( "", "z", "" ), Join3( "", "", "" ) == ""
   ? "in order:", Piece( "1" ) + Piece( "2" ) + Piece( "3" ) + Piece( "4" ), s_cLog
   s_cLog := ""
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      c := Piece( "a" ) + "-" + Num( 5 ) + Piece( "b" )
   RECOVER USING oErr
      ? "fails in place:", oErr:subCode, oErr:operation, Shown( oErr:args ), s_cLog
   END SEQUENCE
   s_cLog := ""
   c := Substituted()
   ? "substituted:", c, s_cLog
   ? "object first:", ( TCount():New() + "a" + "b" + "c" ):cText
   ? "inlined piece:", Outer( 7 ), Outer( 12 )
   ? "unknown types:", Mixed( "p", "q", "r" ), Sum3( 1, 2, 3 ), Sum3( "s", "u", "m" )
   c := ""
   FOR i := 1 TO 3
      c += Str( i, 1 ) + "," + Str( i * i, 1 ) + ";"
   NEXT
   ? "appended:", c
   ? "macro:", &( "'m' + '-' + Upper( 'x' ) + Str( 1, 1 )" )
   ? "dates:", Days( SToD( "20240101" ), 1, 2 )
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      c := "a" + Piece( "b" ) + Num( 5 )
   RECOVER USING oErr
      ? "last piece:", oErr:subCode, Shown( oErr:args )
   END SEQUENCE
   ? "last piece substituted:", LastSubstituted()
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      c := "<" + Piece( "x" ) + SToD( "20240102" ) + Piece( "y" )
   RECOVER USING oErr
      ? "date piece:", oErr:subCode, ValType( oErr:args[ 1 ] ), ValType( oErr:args[ 2 ] ), oErr:args[ 1 ]
   END SEQUENCE
   ? "loop:", Looped( { "a", "bb", "ccc", 4 } )
   ? "grown:", Grown( 4 )
   ? "long:", Len( Long( 40 ) ), Right( Long( 3 ), 12 )
RETURN

// The last `+` fails; its handler substitutes the sum.
STATIC FUNCTION LastSubstituted()
   LOCAL bOld := ErrorBlock( {| e | ValType( e:args[ 1 ] ) + ValType( e:args[ 2 ] ) + e:args[ 1 ] } ), c
   c := "p" + Piece( "q" ) + Num( 7 )
   ErrorBlock( bOld )
RETURN c

// Pieces of an array's elements: strings, then a number, whose `+` fails (its handler substitutes "#").
STATIC FUNCTION Looped( a )
   LOCAL i, c, cAll := "", bOld := ErrorBlock( {|| "#" } )
   FOR i := 1 TO Len( a )
      c := "<" + a[ i ] + ">" + Str( i, 1 )
      cAll += c + LTrim( Str( Len( c ) ) ) + " "
   NEXT
   ErrorBlock( bOld )
RETURN cAll

// The variable is a piece of the chain whose sum it is assigned.
STATIC FUNCTION Grown( n )
   LOCAL c := "", i
   FOR i := 1 TO n
      c := "(" + c + ")" + Str( i, 1 )
   NEXT
RETURN c


// A literal piece: joined whatever the others are.
STATIC FUNCTION Join3( a, b, c )
RETURN a + b + c + ""

STATIC FUNCTION Join5( a, b, c, d, e )
RETURN "" + a + b + c + d + e

// A piece that logs when it is computed.
STATIC FUNCTION Piece( c )
   s_cLog += c
RETURN c

STATIC FUNCTION Num( n )
   s_cLog += "n"
RETURN n

// The handler substitutes "S" for the failing `+`: the chain goes on from it.
STATIC FUNCTION Substituted()
   LOCAL bOld := ErrorBlock( {| e | s_cLog += "E" + LTrim( Str( e:subCode ) ), "S" } ), c
   c := Piece( "a" ) + "b" + Num( 1 ) + Piece( "c" ) + "d"
   ErrorBlock( bOld )
RETURN c

STATIC FUNCTION Outer( n )
RETURN "<" + Inner( n ) + ">" + Inner( n + 1 )

STATIC FUNCTION Inner( n )
RETURN "i" + LTrim( Str( n ) ) + "."

STATIC FUNCTION Mixed( a, b, c )
RETURN a + b + c + "!"

// No piece is a string for sure: the `+`s as they are.
STATIC FUNCTION Sum3( a, b, c )
RETURN a + b + c

// Pieces that are dates and numbers: the chain's `+` of them as each would be.
STATIC FUNCTION Days( d, n, m )
RETURN DToS( d + n + m ) + "/" + DToS( d + m + n )

STATIC FUNCTION Shown( a )
   LOCAL c := "", x
   FOR EACH x IN a
      c += ValType( x ) + ":" + IIf( ValType( x ) == "N", LTrim( Str( x ) ), x ) + " "
   NEXT
RETURN c

CLASS TCount
   VAR cText INIT ""
   VAR nCalls INIT 0
   METHOD New() INLINE Self
   OPERATOR "+" ARG x INLINE ::nCalls++, ::cText := ::cText + x + LTrim( Str( ::nCalls ) ), Self
ENDCLASS

// A chain of 121 pieces (more than one join): each call adds 120 characters.
STATIC FUNCTION Long( n )
RETURN IIf( n == 0, "", Long( n - 1 ) ) + "a" + Str( n % 10, 1 ) + "b" + Str( n % 10, 1 ) + "c" + ;
   Str( n % 10, 1 ) + "d" + Str( n % 10, 1 ) + "e" + Str( n % 10, 1 ) + "f" + Str( n % 10, 1 ) + "g" + ;
   Str( n % 10, 1 ) + "h" + Str( n % 10, 1 ) + "i" + Str( n % 10, 1 ) + "j" + Str( n % 10, 1 ) + "k" + ;
   Str( n % 10, 1 ) + "l" + Str( n % 10, 1 ) + "m" + Str( n % 10, 1 ) + "n" + Str( n % 10, 1 ) + "o" + ;
   Str( n % 10, 1 ) + "p" + Str( n % 10, 1 ) + "q" + Str( n % 10, 1 ) + "r" + Str( n % 10, 1 ) + "s" + ;
   Str( n % 10, 1 ) + "t" + Str( n % 10, 1 ) + "u" + Str( n % 10, 1 ) + "v" + Str( n % 10, 1 ) + "w" + ;
   Str( n % 10, 1 ) + "x" + Str( n % 10, 1 ) + "y" + Str( n % 10, 1 ) + "z" + Str( n % 10, 1 ) + "a" + ;
   Str( n % 10, 1 ) + "b" + Str( n % 10, 1 ) + "c" + Str( n % 10, 1 ) + "d" + Str( n % 10, 1 ) + "e" + ;
   Str( n % 10, 1 ) + "f" + Str( n % 10, 1 ) + "g" + Str( n % 10, 1 ) + "h" + Str( n % 10, 1 ) + "i" + ;
   Str( n % 10, 1 ) + "j" + Str( n % 10, 1 ) + "k" + Str( n % 10, 1 ) + "l" + Str( n % 10, 1 ) + "m" + ;
   Str( n % 10, 1 ) + "n" + Str( n % 10, 1 ) + "o" + Str( n % 10, 1 ) + "p" + Str( n % 10, 1 ) + "q" + ;
   Str( n % 10, 1 ) + "r" + Str( n % 10, 1 ) + "s" + Str( n % 10, 1 ) + "t" + Str( n % 10, 1 ) + "u" + ;
   Str( n % 10, 1 ) + "v" + Str( n % 10, 1 ) + "w" + Str( n % 10, 1 ) + "x" + Str( n % 10, 1 ) + "y" + ;
   Str( n % 10, 1 ) + "z" + Str( n % 10, 1 ) + "a" + Str( n % 10, 1 ) + "b" + Str( n % 10, 1 ) + "c" + ;
   Str( n % 10, 1 ) + "d" + Str( n % 10, 1 ) + "e" + Str( n % 10, 1 ) + "f" + Str( n % 10, 1 ) + "g" + ;
   Str( n % 10, 1 ) + "h" + Str( n % 10, 1 )
