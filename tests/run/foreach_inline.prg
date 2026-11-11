// FOR EACH reads and moves in place over arrays and hashes without holes, and leaves the rest to the runtime: an
// array that grows under the loop (its items move), one that shrinks (the bound error), a hash that gets holes while
// the loop runs, assignments through the variable into an array and a hash, the element passed by reference,
// strings, and the keys of a hash that gets holes.
PROCEDURE Main()
   LOCAL a := { 1, 2, 3 }, h := { "a" => 1, "b" => 2, "c" => 3, "d" => 4 }, x, c := "", e, n := 0, k
   FOR EACH x IN a
      IF Len( a ) < 6
         AAdd( a, x * 10 )
      ENDIF
      c += Str( x, 3 )
   NEXT
   ? "grows:", c
   FOR EACH x IN a
      x := x + 1
   NEXT
   ? "assigned:", a[ 1 ], a[ 6 ]
   c := ""
   FOR EACH x IN h
      IF x == 2
         HDel( h, "a" )
      ENDIF
      c += Str( x, 2 )
   NEXT
   ? "holes:", c, Len( h )
   FOR EACH x IN h
      x *= 100
   NEXT
   ? "hash assigned:", h[ "b" ], h[ "d" ]
   FOR EACH x IN a
      Bump( @x )
   NEXT
   ? "by reference:", a[ 1 ], a[ 2 ]
   c := ""
   FOR EACH x IN "xyz"
      c += Upper( x )
      x := "!"
   NEXT
   ? "string:", c
   c := ""
   FOR EACH k => x IN h
      IF k == "b"
         HDel( h, "c" )
      ENDIF
      c += k + Str( x, 4 ) + " "
   NEXT
   ? "keys:", c
   ErrorBlock( {| o | Break( o ) } )
   BEGIN SEQUENCE
      FOR EACH x IN a
         IF ++n == 2
            ASize( a, 1 )
         ENDIF
         c := x
      NEXT
   RECOVER USING e
      ? "shrinks:", e:description, e:subCode
   END SEQUENCE
RETURN

STATIC PROCEDURE Bump( n )
   n += 1000
RETURN
