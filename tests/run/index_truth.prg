// An element read tested at once as a condition (IF a[ i ], DO WHILE, .NOT.: the test takes the logical a compact
// array holds without the item in between), on every path the read takes, at -O0 and -O2 (run0.*): an array of
// logicals (compact), of items holding logicals, one that becomes compact in the loop and one that stops being so, a
// hash, an element NIL and an element that is not a logical (the condition's error), an index out of range (the
// error), and the read in a loop condition.
PROCEDURE Main()
   LOCAL aCompact := Array( 6 ), aItems := { .T., "x", .F., .T., { 1 }, .F. }, aGrow := Array( 6 ), i, c := ""
   LOCAL hFlags := { "a" => .T., "b" => .F. }, aNil := { .T., NIL, .F. }, k, oErr
   AFill( aCompact, .F. )
   aCompact[ 2 ] := .T.
   aCompact[ 5 ] := .T.
   FOR i := 1 TO 6
      c += IIf( aCompact[ i ], "T", "f" )
   NEXT
   ? "compact:", c
   c := ""
   FOR i := 1 TO 6
      IF ValType( aItems[ i ] ) == "L"
         IF aItems[ i ]
            c += "T"
         ELSE
            c += "f"
         ENDIF
      ENDIF
   NEXT
   ? "items:", c
   c := ""
   FOR i := 1 TO 6
      aGrow[ i ] := i % 2 == 0 // compact once its first logical is stored
      IF aGrow[ i ]
         c += Str( i, 1 )
      ENDIF
      IF i == 4
         aGrow[ 1 ] := "now items"
      ENDIF
   NEXT
   ? "changing form:", c, ValType( aGrow[ 1 ] )
   ? "hash:", IIf( hFlags[ "a" ], "a", "-" ), IIf( !hFlags[ "b" ], "not b", "-" )
   c := ""
   FOR i := 1 TO 3
      BEGIN SEQUENCE WITH {| e | Break( e ) }
         IF aNil[ i ]
            c += Str( i, 1 )
         ENDIF
      RECOVER USING oErr
         c += "[" + oErr:Description + "]"
      END SEQUENCE
   NEXT
   ? "nil:", c
   ? "not a logical:", Tested( aItems, 2 ), "out of range:", Tested( aCompact, 9 )
   k := 1
   DO WHILE aCompact[ k ] .OR. k < 3
      k++
   ENDDO
   ? "loop condition:", k
RETURN

STATIC FUNCTION Tested( a, i )
   LOCAL c := "kept"
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      IF a[ i ]
         c := "true"
      ENDIF
   RECOVER USING c
      c := c:Description
   END SEQUENCE
RETURN c
