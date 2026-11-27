// AS type after a declared name, as Harbour writes it, everywhere it does: LOCAL (with an initializer, a[ n ]), STATIC,
// PRIVATE, PUBLIC, MEMVAR, FIELD, PARAMETERS, a function's, a method's and a codeblock's parameters, a class's VAR,
// with Harbour's names (CHARACTER, CODEBLOCK, USUAL, ANYTYPE, CLASS name, ARRAY OF type, POINTER) and xbase's (INTEGER,
// DOUBLE, ANY, alternatives joined by |). It is a hint, not a check: a variable declared AS NUMERIC holds a string, a
// function whose parameters are AS INTEGER takes doubles and strings and gives what it gives without AS, in loops too.
STATIC s_n AS NUMERIC := 5

MEMVAR p, q

PROCEDURE Main()
   LOCAL n AS NUMERIC := 3, c AS CHARACTER := "x", a[ 2 ] AS ARRAY, l AS LOGICAL := .T., d AS DATE, o AS OBJECT
   LOCAL u AS USUAL, w AS ANYTYPE, b AS CODEBLOCK := {| x AS NUMERIC | x + 1 }, e AS ARRAY OF NUMERIC := { 1, 2 }
   LOCAL k AS CLASS Box, t AS POINTER, i AS INTEGER := 7, f AS DOUBLE := 1.5, y AS NUMERIC|STRING := "y", z AS ANY
   PRIVATE p AS NUMERIC := 1
   PUBLIC q AS STRING := "q"
   n := "now a string"
   ? n, c, Len( a ), l, d, o, u, w, Eval( b, 1 ), Len( e ), k, t, i, f, y, z, s_n, p, q
   ? "integers:", Summed( 4, 3 ), "doubles:", Summed( 2.5, 3 ), "strings:", Joined( "ab", 2 )
   k := Box():New( 6 )
   ? "method:", k:Twice( 4 ), k:nSize
   Params( 9 )
RETURN

FUNCTION Summed( x AS INTEGER, m AS INTEGER )
   LOCAL j, s := 0
   FOR j := 1 TO m
      s += x * j
   NEXT
RETURN s

FUNCTION Joined( x AS INTEGER, m AS INTEGER )
   LOCAL j, s := ""
   FOR j := 1 TO m
      s += x
   NEXT
RETURN s

PROCEDURE Params
   PARAMETERS r AS NUMERIC
   FIELD fName AS CHARACTER
   ? "parameters:", r
RETURN

CLASS Box
   VAR nSize AS NUMERIC INIT 7
   VAR oNext AS CLASS Box
   METHOD New( nSize AS NUMERIC )
   METHOD Twice( x AS NUMERIC ) INLINE x * 2
ENDCLASS

METHOD New( nSize AS NUMERIC ) CLASS Box
   ::nSize := nSize
RETURN Self
