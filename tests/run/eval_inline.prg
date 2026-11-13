// Eval of a codeblock literal, which runs where it is made, behaves as the block's call: the call stack shows the
// block (ProcName, ProcLine, also to a function it calls and to an error), its parameters take the arguments (NIL past
// the last, references through them), its LOCALs start NIL on every pass, it changes the variables it captures and the
// module's STATICs, nests (a block within a block shown with its function's name, where the reference shows one that
// Eval runs from a block as (b)EVAL), runs in loops, and a BREAK or an error out of it leaves the stack as it was.
STATIC s_nCount := 0

PROCEDURE Main()
   LOCAL n := 1, i, s := 0, a, e
   ? Eval( {| x | x + n }, 10 )
   ? Eval( {|| ProcName( 0 ) + "/" + ProcName( 1 ) + " " + Str( ProcLine( 0 ), 3 ) } )
   ? Eval( {|| Where() } )
   ? ProcName( 0 ), ProcLine( 0 )
   ? Eval( {| x, y | ValType( x ) + ValType( y ) }, 1 ), Eval( {| x | x }, 7, 8, 9 )
   Eval( {| r | r += 5 }, @n )
   ? n
   a := { 1, 2 }
   Eval( {| r | r := "changed" }, @a[ 2 ] )
   ? a[ 2 ]
   Eval( {|| n *= 3 } )
   ? n
   FOR i := 1 TO 5
      s := Eval( {| k | s + k * k }, i )
      Eval( {|| s_nCount++ } )
   NEXT
   ? s, s_nCount
   ? Eval( {| x | Eval( {| y | x * 10 + y }, 2 ) }, 1 )
   ? Eval( {|| Eval( {| y | ProcName( 0 ) + "<" + ProcName( 1 ) + "<" + ProcName( 2 ) + Str( y + n, 4 ) }, 2 ) } )
   FOR i := 1 TO 3
      ? Eval( {| x |
         LOCAL t
         t := iif( t == NIL, 0, t ) + x
         RETURN t
         }, i )
   NEXT
   BEGIN SEQUENCE
      Breaks()
   RECOVER USING e
      ? "recovered", e, ProcName( 0 )
   END
   BEGIN SEQUENCE WITH {| o | Break( o ) }
      Fails()
   RECOVER USING e
      ? "error", e:description, e:operation, ProcName( 0 ), ProcLine( 0 )
   END
   ? Eval( {|| ProcName( 0 ) } )
RETURN

FUNCTION Where()
RETURN ProcName( 0 ) + "<" + ProcName( 1 ) + "<" + ProcName( 2 ) + " " + Str( ProcLine( 1 ), 3 )

PROCEDURE Breaks()
   Eval( {|| Break( "out of " + ProcName( 0 ) ) } )
RETURN

PROCEDURE Fails()
   LOCAL x := "a"
   Eval( {|| x - 1 } )
RETURN
