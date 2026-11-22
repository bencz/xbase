// A STATIC initialized with an array literal of constants that no code changes or lets out is a constant table: its
// elements read as constant data (integers, doubles with their decimals, logicals, NIL), by an integer or a double
// index (truncated), Len() of it a constant; an index out of bounds raises the bound error on the STATIC's array, one
// that is no number the error whose handler substitutes the element; a codeblock of the module reads one too. STATICs
// whose array is written or passed on stay arrays. The array an error gives its handler (its arguments) is the
// STATIC's: a change made to it there shows in every read after (the table breaks), as in the array itself.
STATIC s_aNum := { 10, -3, 1.50, 2.125, .T., NIL, 9223372036854775807 }
STATIC s_aWritten := { 1, 2, 3 }
STATIC s_aPassed := { 4, 5, 6 }
STATIC s_aInBlock := { 7, 8, 9 }
STATIC s_aEmpty := {}

PROCEDURE Main()
   LOCAL i, n := 0, x, b, oErr, u
   FOR i := 1 TO Len( s_aNum )
      ? i, s_aNum[ i ], ValType( s_aNum[ i ] )
   NEXT
   ? "double index:", s_aNum[ 2.9 ], s_aNum[ 1.0 ], s_aNum[ 7.5 ]
   FOR i := 1 TO 1000
      n += s_aNum[ i % 2 + 1 ] * 3 + s_aNum[ 3 ] * 2
   NEXT
   ? "sum:", n
   ? "last plus one:", s_aNum[ 7 ] + 1
   FOR EACH x IN { 0, 8, -1, 9223372036854775807 }
      BEGIN SEQUENCE WITH {| e | Break( e ) }
         ? "index", x, s_aNum[ x ]
      RECOVER USING oErr
         ? "index", x, "error", oErr:subCode, oErr:description, Len( oErr:args ), ValType( oErr:args[ 1 ] )
      END SEQUENCE
   NEXT
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      ? s_aEmpty[ 1 ]
   RECOVER USING oErr
      ? "empty:", oErr:subCode, Len( s_aEmpty )
   END SEQUENCE
   ErrorBlock( {| e | "substituted" } )
   ? "not a number:", s_aNum[ "x" ], s_aNum[ u ]
   s_aWritten[ 2 ] := 20
   ? "written:", s_aWritten[ 1 ], s_aWritten[ 2 ], s_aWritten[ 3 ]
   ? "passed:", Total( s_aPassed ), s_aPassed[ 3 ]
   b := {| k | s_aInBlock[ k ] }
   ? "in a block:", Eval( b, 1 ), s_aInBlock[ 3 ]
   ? "broken:", Broken()
RETURN

STATIC FUNCTION Broken()
   LOCAL oErr, i, s := ""
   FOR i := 1 TO 3
      s += Str( s_aInBlock[ i ] )
   NEXT
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      s += Str( s_aInBlock[ 4 ] )
   RECOVER USING oErr
      AAdd( oErr:args[ 1 ], 40 )
      oErr:args[ 1 ][ 1 ] := "changed"
   END SEQUENCE
   FOR i := 1 TO Len( s_aInBlock )
      s += " " + iif( ValType( s_aInBlock[ i ] ) == "C", s_aInBlock[ i ], LTrim( Str( s_aInBlock[ i ] * 2 ) ) )
   NEXT
RETURN s

STATIC FUNCTION Total( a )
   LOCAL x, n := 0
   a[ 1 ] := 40
   FOR EACH x IN a
      n += x
   NEXT
RETURN n
