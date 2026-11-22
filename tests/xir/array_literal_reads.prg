// fold-constants: an element read at a constant position of an array literal that nothing changes or lets out is the
// value it was made with; the codeblock read so is then inlined. An array that the inlined body of Sum2 then only
// holds goes when nothing can see its elements' lifetimes (constants), and stays held when an element may be an
// object (n). A written array keeps its reads.
FUNCTION Reads( n )
   LOCAL aB := { {| x | x + n }, 5 }, b := { 1, 2 }
   b[ 1 ] := n
RETURN Eval( aB[ 1 ], aB[ 2 ] ) + Sum2( { 4, 5 } ) + Sum2( { n, 1 } ) + b[ 1 ]

STATIC FUNCTION Sum2( v )
RETURN v[ 1 ] + v[ 2 ]
