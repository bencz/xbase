// Codeblocks evaluated through their direct entry (as many arguments as parameters, none by reference) and through
// their function (an argument by reference, fewer or more arguments than parameters): a parameter written in the
// block, captured by a nested block, an extended block's LOCALs, a captured variable changed; blocks the runtime
// evaluates (ASort, AScan, AEval, HEval, Eval() reached by a macro); heap values passed while collections run; an
// error raised in a block (the levels its handler sees); blocks evaluated on several threads at once; blocks nested 8
// deep, each capturing the outermost's parameter; a block whose parameter is captured (shared), and one with a shared
// and a plain parameter, given references in compiled and runtime Evals; five arguments to blocks of one and five
// parameters; blocks made and evaluated in another module. A block capturing a parameter of the block around it goes
// beyond the reference, which rejects it.
PROCEDURE Main()
   LOCAL b, n := 10, x := 1, a, h, i, s, oErr, bAdd, aNest, c

   b := {| p | p := p * 2, p + 1 }
   ? "written:", Eval( b, x ), x
   ? "by reference:", Eval( b, @x ), x
   x := 1
   bAdd := {| p, q | IIf( q == NIL, "q nil", p + q ) }
   ? "fewer:", Eval( bAdd, 1 ), "more:", Eval( bAdd, 1, 2, 3 ), "exact:", Eval( bAdd, 1, 2 )
   ? "no parameters:", Eval( {|| n } ), Eval( {|| n }, 5 )

   aNest := {}
   b := {| p | AAdd( aNest, {|| p } ), p := p + 100, p }
   ? "captured parameter:", Eval( b, 1 ), Eval( aNest[ 1 ] ), Eval( b, 2 ), Eval( aNest[ 2 ] )
   b := {| p | n := n + p, n }
   ? "captured variable:", Eval( b, 5 ), Eval( b, 5 ), n
   b := {| p, q |
      LOCAL t := p * q
      t += 1
      RETURN t
      }
   ? "extended:", Eval( b, 3, 4 ), Eval( b, 2.5, 2 )

   a := { 5, 3, 9, 1, 7 }
   ASort( a,,, {| p, q | p > q } )
   ? "ASort:", a[ 1 ], a[ 2 ], a[ 3 ], a[ 4 ], a[ 5 ]
   ? "AScan:", AScan( a, {| p | p < 4 } )
   s := 0
   AEval( a, {| p | s += p } )
   ? "AEval one:", s
   s := ""
   AEval( a, {| p, k | s += Str( p, 2 ) + Str( k, 2 ) } )
   ? "AEval two:", s
   h := { "a" => 1, "b" => 2 }
   s := 0
   HEval( h, {| k, v, p | s += v * p } )
   ? "HEval:", s
   ? "Eval by macro:", &( "Eval" )( {| p, q | p * q }, 6, 7 ), &( "Eval" )( {| p | p := "set" }, @x ), x
   x := 1
   ? "by macro, fewer and more:", &( "Eval" )( {| p, q | ValType( q ) }, 1 ), ;
      &( "Eval" )( {| p | p }, 2, 3, 4 ), &( "Eval" )( {| p | p := 5 }, 1, @x ), x
   ? "nested 8 deep:", Eval( {| x1 | Eval( {| x2 | Eval( {| x3 | Eval( {| x4 | Eval( {| x5 | Eval( ;
      {| x6 | Eval( {| x7 | Eval( {| x8 | x1 + x8 }, x7 + 1 ) }, x6 + 1 ) }, x5 + 1 ) }, x4 + 1 ) }, x3 + 1 ) }, ;
      x2 + 1 ) }, x1 + 1 ) }, 1 )

   a := {}
   b := {| p, q | c := p + Replicate( "x", 100 ), AAdd( a, { c, q } ), Len( a ) }
   FOR i := 1 TO 30000
      Eval( b, LTrim( Str( i ) ), { i } )
      IF i % 10000 == 0
         a := ASize( a, 1 )
         __GcCollect()
      ENDIF
   NEXT
   ? "collections:", Len( a ), Left( a[ 1 ][ 1 ], 5 ), a[ 1 ][ 2 ][ 1 ], Left( c, 5 )

   BEGIN SEQUENCE WITH {| e | Break( e ) }
      Eval( {| p | Where( p ) + p }, 1 )
   RECOVER USING oErr
      ? "error:", oErr:subCode, oErr:operation
   END SEQUENCE
   ? "levels:", Eval( {| p | Where( p ) }, 2 )

   aNest := {}
   bAdd := {| p | AAdd( aNest, {|| p } ), p := p * 10, p }
   x := 2
   ? "all shared, by reference:", Eval( bAdd, @x ), x, &( "Eval" )( bAdd, @x ), x, Eval( aNest[ 2 ] )
   bAdd := {| p, q | AAdd( aNest, {|| q } ), p := p + 1, q := q + 1, p + q }
   x := 1
   n := 10
   ? "mixed, first by reference:", Eval( bAdd, @x, n ), x, n, &( "Eval" )( bAdd, @x, n ), x, n
   ? "mixed, second by reference:", Eval( bAdd, x, @n ), x, n, &( "Eval" )( bAdd, x, @n ), x, n, ;
      Eval( ATail( aNest ) )
   x := 1
   n := 10
   ? "five arguments, one parameter:", Eval( {| p | p * 2 }, 4, 5, 6, 7, 8 ), ;
      Eval( {| p, q, r, s, t | p + q + r + s + t }, 1, 2, 3, 4, 5 )
   ? "other module:", Eval( Tripler(), 5 ), &( "Eval" )( Tripler(), 6 ), EvalThere( {| p | p + 1 }, 7 ), ;
      EvalThere( Tripler(), 8 )

   a := {}
   b := {| p, q | Sum( p, q ) }
   FOR i := 1 TO 4
      AAdd( a, Started( b, i ) )
   NEXT
   s := 0
   FOR EACH c IN a
      s += c:Result
   NEXT
   ? "threads:", s
RETURN

STATIC FUNCTION Started( b, k )
RETURN TaskRun( {|| Running( b, k ) } )

STATIC FUNCTION Running( b, k )
   LOCAL i, n := 0
   FOR i := 1 TO 20000
      n += Eval( b, i, k ) + Eval( {| p | Len( p ) }, { i, k } )
   NEXT
RETURN n

STATIC FUNCTION Sum( p, q )
RETURN p + q

STATIC FUNCTION Where( p )
RETURN ProcName( 1 ) + " " + LTrim( Str( p ) ) + " " + ProcName( 2 )
