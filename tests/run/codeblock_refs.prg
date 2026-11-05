// Codeblock parameters that receive references: several at once, passed on to other blocks and functions, compound
// assignment and ++ through them, elements, STATICs, the block's own captured variable, recursion, FOR EACH variables.
STATIC s := 1

PROCEDURE Main()
   LOCAL x := 1, y := 2, z := 3, a := { 10, 20, 30 }, b, inner, e, n, count := 0, fact

   b := {| p, q, r | p := p + 1, q := q * 10, r := "v" }
   Eval( b, @x, @y, z )
   ? "several", x, y, z

   inner := {| v | v := v + 100 }
   b := {| p | Eval( inner, @p ), p }
   ? "passed on to a block", Eval( b, @x ), x

   b := {| p | Twice( @p ), p }
   ? "passed on to a function", Eval( b, @y ), y

   b := {| p, q | p += 5, q++, p * q }
   x := 1
   y := 1
   ? "compound and ++", Eval( b, @x, @y ), x, y

   b := {| p | p := p * 2 }
   Eval( b, @a[ 2 ] )
   ? "element", a[ 1 ], a[ 2 ], a[ 3 ]
   Eval( b, @s )
   ? "static", s

   n := 5
   b := {| p | p := p + n, n := 0, p }
   ? "own capture", Eval( b, @n ), n

   fact := {| k, acc | IIf( k <= 1, acc, ( acc := acc * k, Eval( fact, k - 1, @acc ) ) ) }
   x := 1
   Eval( fact, 5, @x )
   ? "recursion", x

   b := {| p | p := p * 10 }
   FOR EACH e IN a
      Eval( b, @e )
   NEXT
   ? "for each", a[ 1 ], a[ 2 ], a[ 3 ]

   b := {|| count++ }
   x := 7
   Eval( b, @x, @y )
   ? "extra arguments", x, count
   b := {| p, q | ValType( q ) }
   ? "missing argument", Eval( b, @x )

   b := {| p | p := { 1, 2 }, p[ 1 ] := "set", Len( p ) }
   x := NIL
   ? "array through ref", Eval( b, @x ), x[ 1 ], x[ 2 ]

   b := Maker()
   x := 1
   Eval( b, @x )
   Eval( b, @x )
   ? "block from a function", x
   RETURN

PROCEDURE Twice( v )
   v := v * 2
   RETURN

FUNCTION Maker()
   LOCAL step := 3
   RETURN {| p | p := p + step, step++ }
