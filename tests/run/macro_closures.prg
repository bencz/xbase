// Codeblocks compiled from text (the macro operator) that make codeblocks see the parameters of the codeblocks around
// them, as compiled codeblocks do (Clipper's and the reference's macros see none of them): two and three levels, a
// counter kept in a parameter, a parameter changed after the inner block was made and by the inner block, one received
// by reference, one passed on by reference, and a parameter named as a memory variable. Each line shows the macro's
// result, then the compiled codeblock's.
MEMVAR x
PROCEDURE Main()
   LOCAL b, c, v := "start", n
   PRIVATE x := "memvar"
   ? Both( Eval( Eval( &( "{| x | {| y | x * 10 + y } }" ), 4 ), 2 ), ;
      Eval( Eval( {| x | {| y | x * 10 + y } }, 4 ), 2 ) )
   ? Both( Eval( Eval( Eval( &( "{| a | {| b | {| c | a + b + c } } }" ), 100 ), 20 ), 3 ), ;
      Eval( Eval( Eval( {| a | {| b | {| c | a + b + c } } }, 100 ), 20 ), 3 ) )
   b := Eval( &( "{| n | {|| ++n } }" ), 0 )
   c := Eval( {| n | {|| ++n } }, 0 )
   ? Both( Str( Eval( b ), 2 ) + Str( Eval( b ), 2 ) + Str( Eval( b ), 2 ), ;
      Str( Eval( c ), 2 ) + Str( Eval( c ), 2 ) + Str( Eval( c ), 2 ) )
   ? Both( Eval( &( "{| p | c := {|| p }, p := 5, Eval( c ) }" ), 1 ), ;
      Eval( {| p | c := {|| p }, p := 5, Eval( c ) }, 1 ) )
   ? Both( Eval( &( "{| p | Eval( {|| p := p * 3 } ), p }" ), 7 ), Eval( {| p | Eval( {|| p := p * 3 } ), p }, 7 ) )
   Eval( &( "{| r | Eval( {|| r := 'set by macro' } ) }" ), @v )
   n := v
   v := "start"
   Eval( {| r | Eval( {|| r := "set by macro" } ) }, @v )
   ? Both( n, v )
   ? Both( Eval( &( "{| q | Eval( {|| Bump( @q ) } ), q }" ), 10 ), Eval( {| q | Eval( {|| Bump( @q ) } ), q }, 10 ) )
   ? Both( Eval( Eval( &( "{| x | {|| x } }" ), "parameter" ) ), Eval( Eval( {| x | {|| x } }, "parameter" ) ) ), x
   ? Both( Eval( &( "{|| {|| x } }" ) ):Eval(), Eval( {|| {|| M->x } } ):Eval() )
RETURN

STATIC FUNCTION Both( xMacro, xCompiled )
RETURN Transform( xMacro, "" ) + " | " + Transform( xCompiled, "" ) + iif( xMacro == xCompiled, "", "  DIFFERENT" )

PROCEDURE Bump( v )
   v += 100
RETURN
