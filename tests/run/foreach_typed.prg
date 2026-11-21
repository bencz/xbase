// FOR EACH loops versioned by the type of their collection (version-each: a copy for a string, one for an array, one
// for the rest), and the reads of a loop variable that take the element from the source in place (a string's byte,
// an array's element) or through the loop's reference, at -O0 and -O2 (run0.*). One function goes through a string,
// an array, a hash, a generator and an empty string; Asc() and comparisons with a one-character literal on a string's
// byte (both orders, = == != <>); a variable assigned in the loop (its reads then go through the reference: the
// string unchanged, the array's element changed); an element the body changes through the array, read after; an
// array that shrinks during a DESCEND loop (the bound error, recovered); EXIT and the variable after the loop; one
// variable in two nested loops (a variable of the function alone: read in place, and through the outer loop's
// reference after the inner one); a loop variable read by a codeblock (shared: never read in place); an assignment on
// one path of the body; BREAK out of a loop in a sequence (the variable restored); a read in place after a value is
// let go; DESCEND in a sequence.
PROCEDURE Main()
   LOCAL a, x, c, n, b, e
   ? "kinds:", Walk( "abc" ), Walk( { 1, "two", 3 } ), Walk( { "k" => "v", "l" => 9 } ), Walk( Gen() ), ;
      "[" + Walk( "" ) + "]"
   ? "bytes:", Bytes( "Az" + Chr( 0 ) + Chr( 255 ) ), Count( "banana", "a" ), Count( "banana", "n" ), Count( "", "a" )
   ? "compare:", Compare( "xyx" )
   a := "hello"
   c := ""
   FOR EACH x IN a
      x := Upper( x )
      c += x
   NEXT
   ? "assigned:", c, a
   a := { 1, 2, 3 }
   FOR EACH x IN a
      x := x * 10
   NEXT
   ? "array assigned:", a[ 1 ], a[ 2 ], a[ 3 ]
   a := { 1, 2, 3 }
   c := ""
   FOR EACH x IN a
      a[ 3 ] := 30
      c += Str( x, 3 )
   NEXT
   ? "changed through the array:", c
   a := { 1, 2, 3, 4, 5 }
   c := ""
   BEGIN SEQUENCE WITH {| o | Break( o ) }
      FOR EACH x IN a DESCEND
         c += Str( x, 2 )
         ASize( a, 2 )
      NEXT
   RECOVER USING e
      c += " error " + LTrim( Str( e:subCode ) )
   END SEQUENCE
   ? "shrunk:", c
   x := "before"
   FOR EACH x IN "abcdef"
      IF x == "d"
         EXIT
      ENDIF
   NEXT
   ? "after exit:", x
   c := ""
   FOR EACH x IN "ab"
      c += x
      FOR EACH x IN { 1, 2 }
         c += Str( x, 1 )
      NEXT
      c += x
   NEXT
   ? "one variable nested:", c, Nested( "ab" ), Nested( "xyz" )
   c := ""
   b := {|| x }
   FOR EACH x IN "pq"
      c += x + Eval( b )
   NEXT
   ? "captured:", c
   FOR EACH a IN { "abc", { "a", "b", "c" }, { 1, 2 } } // arguments of types no call shows
      ? "assigned on one path:", OnePath( a ), "restored on BREAK:", Broken( a ), ;
         "read after a release:", Released( a ), "descend in a sequence:", Backwards( a )
   NEXT
   n := 0
   FOR EACH x IN Replicate( "0123456789", 100 )
      n += Asc( x ) - 48
   NEXT
   ? "sum:", n
RETURN

// The same loop over whatever it is given: the loop's copies for a string, an array and the rest.
STATIC FUNCTION Walk( xColl )
   LOCAL c := "", x
   FOR EACH x IN xColl
      c += IIf( ValType( x ) == "C", x, LTrim( Str( x ) ) ) + "."
   NEXT
RETURN c

// One variable of the function alone in two nested loops: after the inner one, the outer's byte through its reference.
STATIC FUNCTION Nested( s )
   LOCAL c := "", x
   FOR EACH x IN s
      c += x
      FOR EACH x IN { 1, 2 }
         c += Str( x, 1 )
      NEXT
      c += x + Chr( Asc( x ) + 1 )
   NEXT
RETURN c

// Functions of the program (not STATIC: no copy typed for a call's arguments), whose loops are versioned.
// An assignment on one path of the body: the reads after the paths meet go through the reference.
FUNCTION OnePath( xColl )
   LOCAL c, r := ""
   FOR EACH c IN xColl
      IF ValType( c ) == "N"
         c := Str( c, 1 )
      ELSEIF c == "b"
         c := "X"
      ENDIF
      r += c
   NEXT
RETURN r

// BREAK out of the loop: the variable gets its value from before the loop back (each version restores its own).
FUNCTION Broken( xColl )
   LOCAL x := "before"
   BEGIN SEQUENCE
      FOR EACH x IN xColl
         IF ValType( x ) == "N" .OR. x == "b"
            BREAK
         ENDIF
      NEXT
   END SEQUENCE
RETURN x

// A value let go (frame.release) right before a read in place and an EXIT: the source stays live for the read.
FUNCTION Released( xColl )
   LOCAL c, r := "", o := { 1 }
   FOR EACH c IN xColl
      IF ValType( c ) == "C" .AND. c == "b"
         o := NIL
         r := Replicate( "q", 5 ) + Str( Len( r ), 1 )
         r := c + Chr( Asc( c ) + 1 ) + r
         EXIT
      ENDIF
   NEXT
RETURN r + ValType( o )

// DESCEND inside a sequence (the loop's cleanup and its lengths before it).
FUNCTION Backwards( xColl )
   LOCAL x, r := ""
   BEGIN SEQUENCE
      FOR EACH x IN xColl DESCEND
         r += IIf( ValType( x ) == "N", Str( x, 1 ), x )
      NEXT
   END SEQUENCE
RETURN r

STATIC FUNCTION Gen()
   YIELD 7
   YIELD 8
RETURN

STATIC FUNCTION Bytes( s )
   LOCAL n := "", c
   FOR EACH c IN s
      n += LTrim( Str( Asc( c ) ) ) + " "
   NEXT
RETURN n

STATIC FUNCTION Count( s, cWhat )
   LOCAL n := 0, c
   FOR EACH c IN s
      IF c == cWhat
         n++
      ENDIF
   NEXT
RETURN n

STATIC FUNCTION Compare( s )
   LOCAL r := "", c
   FOR EACH c IN s
      r += IIf( c == "x", "1", "0" ) + IIf( "x" == c, "1", "0" ) + IIf( c = "x", "1", "0" ) + ;
         IIf( c != "x", "1", "0" ) + IIf( c <> "y", "1", "0" ) + " "
   NEXT
RETURN r
