// Codeblocks read as text (the command line: what a user types) and compiled at run time, run with Eval: with no
// arguments, a number, a double, a string, an array, NIL and two arguments, by reference, kept in an array and run by
// AEval, passed to and returned by functions (small ones, which run in place), and a block that makes blocks; text
// that is no block, a syntax error, and the errors the blocks raise (division by zero, an unknown function, a wrong
// type), each recovered with its description and operation.
PROCEDURE Main( ... )
   LOCAL aText := { ... }, aBlocks := {}, cText, b, x, aArgs, n
   aArgs := { {}, { 5 }, { 2.01 }, { "abc" }, { { 1, 2, 3 } }, { NIL }, { 2.01, 5 } }
   FOR EACH cText IN aText
      ? "[" + cText + "]"
      b := Compile( cText )
      IF ValType( b ) != "B"
         ?? " gives", Shown( b )
         LOOP
      ENDIF
      AAdd( aBlocks, b )
      FOR EACH x IN aArgs
         ?? " " + Run( b, x )
      NEXT
   NEXT
   x := 7
   n := Eval( aBlocks[ 11 ], @x )
   ? "by reference", x, n
   ? "kept", Len( aBlocks ), Shown( Eval( aBlocks[ 1 ], 21 ) ), Shown( Apply( aBlocks[ 2 ], 3, 4 ) )
   x := 0
   AEval( { 1, 2, 3 }, {| v | x += Eval( aBlocks[ 1 ], v ) } )
   ? "aeval", x, Run( Eval( aBlocks[ 10 ], 4 ), { 2 } ), Shown( Twice( aBlocks[ 1 ] ) )
RETURN

// The block (or the value) the text compiles to; the error's text when it does not compile.
STATIC FUNCTION Compile( cText )
   LOCAL b, e
   BEGIN SEQUENCE WITH {| o | Break( o ) }
      b := &( cText )
   RECOVER USING e
      b := "error: " + e:description
   END
RETURN b

// Eval( b, ... ) with the arguments in aArgs, its result shown, or the error it raised.
STATIC FUNCTION Run( b, aArgs )
   LOCAL r, e
   BEGIN SEQUENCE WITH {| o | Break( o ) }
      DO CASE
      CASE Len( aArgs ) == 0
         r := Eval( b )
      CASE Len( aArgs ) == 1
         r := Eval( b, aArgs[ 1 ] )
      OTHERWISE
         r := Eval( b, aArgs[ 1 ], aArgs[ 2 ] )
      ENDCASE
   RECOVER USING e
      RETURN "<" + e:description + " " + e:operation + ">"
   END
RETURN Shown( r )

STATIC FUNCTION Apply( b, x, y )
RETURN Eval( b, x, y )

STATIC FUNCTION Twice( b )
RETURN { Eval( b, 1 ), Eval( b, 2 ) }

STATIC FUNCTION Shown( x )
   DO CASE
   CASE ValType( x ) == "A"
      RETURN "{" + LTrim( Str( Len( x ) ) ) + "}"
   CASE ValType( x ) == "B"
      RETURN "block"
   CASE ValType( x ) == "N"
      RETURN LTrim( Transform( x, "" ) )
   CASE ValType( x ) == "U"
      RETURN "NIL"
   ENDCASE
RETURN Transform( x, "" )
