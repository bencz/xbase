// The built-in types as classes (runtime/src/rtl/collections.prg): arrays, strings and hashes inherit Enumerable
// (Select, Where, Take, Skip are generators: lazy, chained; ToArray, Count, Any, All, First, Contains, Aggregate, Sum,
// ForEach), and every type's SCALAR CLASS gives its functions as messages (a:Len(), "x":Upper(), h:Keys(), n:Round(),
// d:Year(), b:Eval()), while ClassName(), ValType() and the functions stay Clipper's, and a message the type's class
// does not answer keeps Clipper's error. A generator's enumerator answers Enumerable's messages, a class that inherits
// Enumerable needs only GetEnumerator(), and AEval and AScan go through such collections too (AScan, AFill, ACopy and
// HEval are written in xBase now; the other tests check them against the reference).
PROCEDURE Main()
   LOCAL a := { 5, 3, 8, 1 }, h := { "a" => 1, "b" => 2 }, x, c := ""
   ? a:Len(), a:ClassName(), "abc":ClassName(), ValType( a ), Len( a )
   ? a:Select( {| x | x * 10 } ):ToArray()[ 2 ], a:Where( {| x | x > 2 } ):Count(), a:Sum(), a:Any( {| x | x > 7 } )
   FOR EACH x IN a:Where( {| x | x % 2 == 1 } ):Select( {| x, i | Str( x, 1 ) + "@" + Str( i, 1 ) } )
      c += x + " "
   NEXT
   ? "lazy:", c
   ? "string:", "hello world":Upper(), "a,b,c":Split( "," ):Len(), "abc":Select( {| ch | ch + ch } ):ToArray()[ 3 ]
   ? "hash:", h:Keys()[ 2 ], h:Sum(), h:Count(), h:HasKey( "b" )
   ? "chain:", a:Clone():Sort():ToArray()[ 1 ], a[ 1 ], a:Add( 9 ):Len(), a:Take( 2 ):Sum(), a:Skip( 3 ):ToArray()[ 1 ]
   ? "number:", ( 3.14159 ):Round( 2 ), ( -5 ):Abs(), ( 42 ):Str(), Date():ClassName()
   ? "block:", {| a, b | a + b }:Eval( 1, 2 )
   ? "aggregate:", a:Aggregate( {| acc, x | acc * x } ), a:First( {| x | x > 5 } ), a:Contains( 8 ), a:Contains( "8" )
   ? "aeval gen:", AEval( a:Select( {| x | x } ), {| x, i | QQOut( Str( x, 2 ) ) } ):ClassName()
   ? "user:", TBag():New():Where( {| x | x > 1 } ):ToArray()[ 1 ]
   ? "generator msgs:", Gen():ToArray()[ 3 ], Gen():Count()
   ? "ascan objects:", AScan( Gen(), 2 ), AScan( Gen(), {| x | x > 2 } ), AScan( TBag():New(), 3, 2 ), ;
      AScan( Gen(), 1, 2 ), AScan( Gen(), 3, 1, 2 ), AScan( Gen(), "1" )
   ? "valtype:", ValType( a ) == "A", ValType( "x" ) = "C", ValType( 1 ) != "N", "a" == "a", "a" = "b"
   ErrorBlock( {| e | QOut( "error:", e:description, e:operation ), Break( e ) } )
   BEGIN SEQUENCE
      ? a:Nope()
   END SEQUENCE
RETURN

FUNCTION Gen()
   YIELD 1
   YIELD 2
   YIELD 3
RETURN

CLASS TBag INHERIT Enumerable
   METHOD GetEnumerator()
ENDCLASS

METHOD GetEnumerator() CLASS TBag
   YIELD 1
   YIELD 2
   YIELD 3
RETURN
