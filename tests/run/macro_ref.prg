// @&name: an argument passed by reference to what the text designates: a PRIVATE or PUBLIC variable (any letter case,
// blanks around, `&name.`, `&( expr )`), an element (the text "a[1]", or @&name[ 2 ], an element of the macro's
// value), through Eval() and DO ... WITH; a function that does not take a reference receives the value. Errors: a
// variable that does not exist (1003; it is not created) or that is a LOCAL (a macro sees none), a text that is
// neither a variable nor an element (1449, also a message), a value that is not a string (1065). Also M->name, an
// element of an element and a hash's value.
MEMVAR x, c, a, pub, h, m

PROCEDURE Main()
   LOCAL e, n, l := "local"
   PRIVATE x := 1, c := "x", a := { 10, 20 }
   PUBLIC pub := 5
   PRIVATE h := { "k" => 1 }, m := { { 1, 2 }, { 3, 4 } }
   Inc( @&c )
   ? "private:", x
   c := "pub"
   Inc( @&c )
   ? "public:", pub
   c := " X "
   Inc( @&c )
   ? "blanks and case:", x
   c := "x"
   Inc( @&c. )
   Inc( @&( "x" ) )
   ? "dot and parentheses:", x
   c := "a"
   Inc( @&c[ 2 ] )
   ? "element of the value:", a[ 2 ]
   c := "x"
   ? "eval:", Eval( {| v | v := 100 }, @&c ), x
   DO Inc WITH @&c
   ? "do with:", x, "len:", Len( @&( "a" ) )
   ErrorBlock( {| o | Break( o ) } )
   FOR EACH n IN { "nosuch", "a[1]", "x+1", 12, "", "l", "M->x", "m[2][1]", "h['k']", "a[1]:x", "h['new']" }
      c := n
      BEGIN SEQUENCE
         Inc( @&c )
         ? "ok", n, x, a[ 1 ], m[ 2, 1 ], h[ "k" ], Len( h ), Type( "nosuch" )
      RECOVER USING e
         ? "error", e:subCode, e:operation, e:description
      END SEQUENCE
   NEXT
   ? l
RETURN

STATIC PROCEDURE Inc( v )
   v := iif( v == NIL, 0, v ) + 1
RETURN
