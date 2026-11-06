// x := x + y and x += y on a LOCAL that holds strings grow the string in place while nothing else holds it
// (xb_op_append): every copy taken before an append (another variable, an argument, an element, a hash key, a
// returned value, a codeblock's view, an error's arguments) keeps its value; other types, errors and their
// substitution behave as +; the collector runs between appends.
PROCEDURE Main()
   LOCAL x := "", t, a := {}, h := {=>}, i, e, n, five := 5, yes := .T.

   FOR i := 1 TO 10
      x += Chr( 64 + i )
   NEXT
   ? "grow", x, Len( x )

   t := x
   x += "-1"
   AAdd( a, x )
   x := x + "-2"
   h[ x ] := 1
   x += "-3"
   ? "copies", t, a[ 1 ], HKeyAt( h, 1 ), x

   x := "ab"
   x := x + x
   x += x
   ? "self", x
   x := "q"
   x := x + ( x := "z" )
   ? "assign inside", x
   x := Kept( x )
   x += "!"
   ? "returned", x

   x := ""
   FOR EACH t IN { "a", "b", "c" }
      x += t
   NEXT
   n := 0
   FOR EACH t IN x
      n++
      x += "."
   NEXT
   ? "walked", n, x

   // Errors: the variable changes only when + gives a value.
   x := "s"
   x += "t"
   ErrorBlock( {| err | Break( err ) } )
   BEGIN SEQUENCE
      x += five
   RECOVER USING e
      ? "error", e:subCode, e:operation, e:args[ 1 ], e:args[ 2 ], x
   END
   x += "u"
   ? "after", x
   ErrorBlock( {| err | "SUB" } )
   x += yes
   ? "substituted", x
   x += "!"
   ? "then", x

   // Large strings with collections in between, and a final check of every byte.
   x := ""
   FOR i := 1 TO 20000
      x += Chr( 65 + i % 26 ) + Replicate( "-", i % 3 )
      IF i % 5000 == 0
         __GcCollect()
      ENDIF
   NEXT
   n := 0
   FOR i := 1 TO Len( x )
      n += Asc( SubStr( x, i, 1 ) )
   NEXT
   ? "large", Len( x ), n, Left( x, 8 ), Right( x, 4 )

   // Read in a loop that also appends to it: the growth ends at the reads and starts again with a new string; one
   // byte, several bytes and NUL bytes are copied alike.
   x := ""
   FOR i := 1 TO 50
      x += "ab"
   NEXT
   t := ""
   FOR i := 1 TO Len( x )
      t += SubStr( x, i, 1 )
      IF i % 10 == 0
         x += Chr( 0 ) + "Z"
      ENDIF
   NEXT
   n := 0
   FOR i := 1 TO Len( x )
      n += Asc( SubStr( x, i, 1 ) ) * i
   NEXT
   ? "reread", Len( x ), Len( t ), n, t == x, Len( x ) - Len( StrTran( x, Chr( 0 ) ) ), At( Chr( 0 ) + "Z", x )

   // Len() and Asc() read the growing string in place: a loop that tests its length keeps growing it.
   x := ""
   DO WHILE Len( x ) < 100000 .AND. Asc( x ) != 63
      x += "q"
   ENDDO
   ? "while", Len( x ), Right( x, 2 )

   // A handler block is entered from the middle of the sequence's body: reading there ends the growth too.
   x := ""
   ErrorBlock( {| err | Break( err ) } )
   BEGIN SEQUENCE
      x += "ab"
      x += "cd"
      Boom()
      t := x
   RECOVER
      t := x
      x += "Z"
   END
   ? "handler", t, x
   ErrorBlock( {| err | Break( err ) } )
   BlockView()
   OtherTypes()
RETURN

// A variable that also holds numbers is not grown in place (a numeric accumulator stays in registers); + as usual.
STATIC PROCEDURE OtherTypes()
   LOCAL x := ""
   x := 5
   x += 1
   x := x + 2.5
   ? "numbers", x
   x := "a"
   x += "b"
   ? "then", x
RETURN

// A variable a codeblock sees is shared: plain +.
STATIC PROCEDURE BlockView()
   LOCAL x := "b", blk := {|| x }, t
   x += "c"
   t := Eval( blk )
   x += "d"
   ? "block", t, Eval( blk )
RETURN

STATIC PROCEDURE Boom()
   Eval( {|| 1 + "a" } )
RETURN

STATIC FUNCTION Kept( s )
   LOCAL r := s
   r += "?"
RETURN r
