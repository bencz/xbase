// Function references (@name()): a symbol, ValType "S", shown as @NAME(), not empty, equal (==) to another reference
// to the same function, run by Eval(), Do() and :Exec(), named by :Name; of the program's functions, STATIC ones too,
// and of the runtime's; kept in arrays and hashes, passed and returned; a STATIC FUNCTION referenced only so and given
// a variable by reference through Eval(); recursion through a reference; PCount() of a call through Do(); a runtime
// function that takes a reference; in macros (a missing name is error 1001); Type(), ClassName() and AScan() (by ==,
// which no reference matches unless a block compares); STATIC initializers (a dispatch table); DO ... WITH a reference
// and a message to it; $ of a pair whose value is a reference; and the errors of operators on them.
MEMVAR pCount
STATIC s_aOps := { @Twice(), @Neg() }, s_p := @Square()

PROCEDURE Main()
   LOCAL p := @Twice(), q := @Twice(), aOps, h, x := 5, c, e, n
   PRIVATE pCount := @Counted()
   ? ValType( p ), p, Empty( p ), p == q, p == @Str(), p:Name, @Str():Name, Len( { p } )
   ? Eval( p, 21 ), Do( p, 4 ), p:Exec( 5 ), Eval( p ), Do( @Upper(), "abc" ), Eval( @Str(), 7, 3 )
   aOps := { @Twice(), @Square(), @Neg() }
   FOR EACH c IN aOps
      ?? "", c:Name, Eval( c, 3 )
   NEXT
   h := { "twice" => @Twice(), "square" => @Square() }
   ? Eval( h[ "square" ], 9 ), Apply( @Neg(), 4 ), Eval( Pick( 2 ), 6 ), ValType( Pick( 1 ) )
   Eval( @Bump(), @x )
   ? "by reference:", x
   Eval( @Bump(), x )
   ? "by value:", x
   ? "recursion:", Eval( @Fact(), 10 )
   ? "pcount:", Do( pCount ), Do( pCount, 1, 2, 3 ), Eval( M->pCount, 1 )
   c := "abc"
   Eval( @StuffIn(), @c )
   ? "native by reference:", c
   ? "macro:", &( "@Str()" ):Name, Eval( &( "@Upper()" ), "x" ), ValType( &( "@Twice()" ) )
   ? "type:", Type( "@Twice()" ), p:ClassName(), AScan( aOps, aOps[ 2 ] ), AScan( aOps, {| f | f == @Neg() } )
   ? "static:", Eval( s_aOps[ 1 ], 8 ), Eval( s_aOps[ 2 ], 8 ), Eval( s_p, 8 ), Type( "@NoSuchFunction()" )
   DO Shown WITH @Neg():Name, @Neg()
   ? "pair:", { "f" => @Neg() } $ { "f" => @Neg(), "g" => 1 }, { "f" => @Neg() } $ { "f" => @Twice() }
   ErrorBlock( {| o | Break( o ) } )
   FOR EACH n IN { {|| &( "@NoSuchFunction()" ) }, {|| p + 1 }, {|| p < q }, {|| p = q }, {|| { p => 1 } } }
      BEGIN SEQUENCE
         ? Eval( n )
      RECOVER USING e
         ? "error", e:subCode, e:operation
      END SEQUENCE
   NEXT
RETURN

FUNCTION Twice( n )
RETURN iif( n == NIL, "nil", n * 2 )

STATIC FUNCTION Square( n )
RETURN n * n

STATIC FUNCTION Neg( n )
RETURN -n

STATIC FUNCTION Apply( f, x )
RETURN Eval( f, x )

STATIC FUNCTION Pick( k )
RETURN { @Square(), @Neg() }[ k ]

// Referenced only as @Bump(): its callers are unknown, and one passes a reference.
STATIC PROCEDURE Bump( n )
   n++
RETURN

STATIC FUNCTION Fact( n )
RETURN iif( n <= 1, 1, n * Eval( @Fact(), n - 1 ) )

STATIC FUNCTION Counted( ... )
RETURN PCount()

STATIC PROCEDURE Shown( cName, f )
   ? "do with:", cName, f
RETURN

STATIC PROCEDURE StuffIn( c )
   c := Stuff( c, 2, 0, "-" )
RETURN
