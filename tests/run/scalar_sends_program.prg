// A program's SCALAR CLASS ARRAY answers its messages before the runtime's (resolve-scalar-sends calls the runtime's
// methods behind a test of that), at -O0 and -O2 (run0.*): Where is the program's for arrays, Enumerable's for strings;
// what the program's class does not answer (Select, Sum) is still the runtime's; the chain on an array goes through the
// program's Where.
PROCEDURE Main()
   ? "array:", { 1, 2, 3 }:Where( {| v | v > 1 } ):Count(), { 1, 2, 3 }:Select( {| v | v * 2 } ):Sum()
   ? "string:", "abc":Where( {| c | c > "a" } ):Count(), "abc":Select( {| c | c + c } ):ToArray()[ 3 ]
   ? "chain:", Chain( { 5, 6, 7 } ), Chain( "pq" )
RETURN

FUNCTION Chain( x )
RETURN x:Where( {| v | .T. } ):Select( {| v | v } ):Count()

SCALAR CLASS ARRAY
   METHOD Where( b ) INLINE { "program's", Len( Self ) }
ENDCLASS
