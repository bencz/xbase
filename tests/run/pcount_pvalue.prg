// PCount() and PValue(): the arguments a function or codeblock received, its parameters by their current value (a
// function calling PValue() keeps them in its frame) and the arguments past them as passed; PValue( n, x ) writes one,
// through the reference when one was passed, and the caller's own values stay as they were. Main receives the
// command-line arguments (pcount_pvalue.args), as an INIT procedure does before it. Through a macro: an expression's
// PCount() is the function's running it, a codeblock's its own.
INIT PROCEDURE Setup()
   ? "init", PCount(), PValue( 1 ), PValue( 1, "changed" ), PValue( 2, "too" ), PValue( 1 )
RETURN

PROCEDURE Main( cFirst )
   LOCAL x := 1, y := 2
   ? "main", PCount(), cFirst, PValue( 2 )
   ? "fixed", Fixed( 1, 2, 3, 4 )
   ? "short", Fixed( 1 )
   ? "none", Fixed()
   ? "positions", Positions( "a", "b" )
   ? "writes", Writes( 1, 2, 3 )
   ? "do", Do( "PCOUNT" ), Do( "PVALUE", 1 ), ViaDo( 7 )
   Refs( @x, @y )
   ? "refs", x, y
   ? "blocks", Eval( {| a, b | PCount() }, 1, 2, 3 ), Eval( {| a | PValue( 2 ) }, 1, "two" ), Eval( {|| PCount() } ), ;
      Eval( {| a | a := "set", PValue( 1 ) }, 1 ), Eval( {| a | PValue( 1, "new" ), a }, 1 )
   ? "captured", Captured( 5 )
   ? "nested", Outer( "o1", "o2" )
   ? "promoted", Promoted( 10 )
   ? "abbreviated", PCou()
   ? "macro", ThroughMacro( 1, 2, 3 ), Eval( &( "{| x | PCount() }" ) ), Eval( &( "{| x | PCount() }" ), 9, 8 ), ;
      Eval( &( "{|| PValue( 2 ) }" ), "a", "b" )
   ? "hmerge", Merged()
RETURN

// The filter block replaces its arguments: the entry it was given is the one merged.
FUNCTION Merged()
   LOCAL d := { => }
   HMerge( d, { "a" => 1, "b" => 2 }, {|| PValue( 1, "zz" ), PValue( 2, { 1 } ), .T. } )
RETURN HKeyAt( d, 1 ) + HKeyAt( d, 2 ) + Str( HValueAt( d, 1 ), 2 ) + Str( HValueAt( d, 2 ), 2 ) + Str( Len( d ), 2 )

FUNCTION Fixed( a, b )
   b := "B"
RETURN Str( PCount(), 1 ) + ValType( PValue( 1 ) ) + ValType( PValue( 2 ) ) + ValType( PValue( 3 ) ) + ;
   ValType( PValue( 4 ) ) + ValType( PValue( 5 ) )

FUNCTION Positions( a )
   LOCAL r := ""
   r += ValType( PValue( 0 ) ) + ValType( PValue( -1 ) ) + ValType( PValue( "x" ) ) + ValType( PValue( 3 ) )
   r += PValue( 1.7 ) + PValue( 2.2 ) + ValType( PValue() )
RETURN r

FUNCTION Writes( a )
   LOCAL r := { PValue( 1, "z" ), PValue( 3, "w" ), PValue( 9, "q" ), PValue( 3 ), a }
RETURN r[ 1 ] + r[ 2 ] + ValType( r[ 3 ] ) + r[ 4 ] + r[ 5 ]

// Do shows on the call stack with its own arguments: PCount() and PValue() called through it see them.
FUNCTION ViaDo( n )
RETURN Str( Do( "PCOUNT" ), 1 ) + ValType( Do( "PVALUE", 1 ) ) + Str( n, 1 )

FUNCTION Refs( a )
   a := 10
   PValue( 2, 20 )
RETURN NIL

FUNCTION Captured( n )
   LOCAL b := {|| n }
   PValue( 1, 7 )
RETURN Eval( b ) + PValue( 1 )

FUNCTION Outer( a, b )
RETURN Inner( a + "!" ) + "/" + Str( PCount(), 1 ) + PValue( 2 )

FUNCTION Inner( a )
RETURN Str( PCount(), 1 ) + PValue( 1 )

// No PValue() call: the parameter is an SSA value; PCount() still counts.
FUNCTION Promoted( n )
   LOCAL i, s := 0
   FOR i := 1 TO n
      s += i
   NEXT
RETURN s + PCount()

STATIC FUNCTION ThroughMacro( a, b, c )
RETURN &( "PCount()" )
