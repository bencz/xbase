// A codeblock or reference that takes a FOR EACH variable keeps the current element, also when an inner loop on the
// same variable (here or in a callee) runs next; a parameter that received a FOR EACH variable is a loop variable of
// its own.
PROCEDURE Main()
   LOCAL a := { 1, 2 }, b := { "x", "y" }, x, bl
   FOR EACH x IN a
      bl := {|| x }
      FOR EACH x IN b
         ?? Eval( bl )
      NEXT
   NEXT
   ? "callee:"
   FOR EACH x IN a
      bl := {|| x }
      Inner( @x, b, bl )
   NEXT
   ? "param of loop var:"
   FOR EACH x IN { 1 }
      Q( @x )
   NEXT
   RETURN

PROCEDURE Inner( v, b, bl )
   FOR EACH v IN b
      ?? Eval( bl )
   NEXT
   RETURN

PROCEDURE Q( q )
   LOCAL bl
   FOR EACH q IN { "a", "b" }
      bl := {|| q }
   NEXT
   ?? q, Eval( bl )
   RETURN
