// Calls inlined, and generators fused, inside BEGIN SEQUENCE and TRY, at -O0 and -O2 (run0.*): the body unwinds through
// a block that leaves its frame, so the handler runs in the caller's (the output is the one the calls give): an error
// in an inlined function and in one inlined in it (ProcName() and ProcLine() of each frame, from the error block and
// in RECOVER), a BREAK out of an inlined function, a value returned normally, ALWAYS and FINALLY after an error in an
// inlined body, an error in a fused generator's body and in the loop's body (the loop variable restored), a BREAK
// out of a fused loop, a sequence inside a loop over a generator, and the call stack after RECOVER.
PROCEDURE Main()
   LOCAL e, c, x := "kept"
   BEGIN SEQUENCE WITH {| o | Break( { o:Description, ProcName( 1 ), ProcLine( 1 ), ProcName( 2 ), ProcName( 3 ) } ) }
      ? "value:", Twice( 4 )
      ? Twice( "a" )
   RECOVER USING e
      ? "inlined error:", e[ 1 ], e[ 2 ], e[ 3 ], e[ 4 ], e[ 5 ], "in", ProcName()
   END SEQUENCE
   BEGIN SEQUENCE WITH {| o | Break( { ProcName( 1 ), ProcName( 2 ), ProcName( 3 ) } ) }
      ? Outer( "b" )
   RECOVER USING e
      ? "nested inlined error:", e[ 1 ], e[ 2 ], e[ 3 ]
   END SEQUENCE
   BEGIN SEQUENCE
      ? Breaks( 7 )
   RECOVER USING e
      ? "break from inlined:", e, ProcName()
   END SEQUENCE
   c := ""
   BEGIN SEQUENCE WITH {| o | Break( o ) }
      c += Str( Twice( 1 ), 1 )
      c += Twice( "x" )
   RECOVER
      c += " recovered"
   ALWAYS
      c += " always"
   END SEQUENCE
   ? "always:", c
   c := ""
   TRY
      c += Str( Twice( 2 ), 1 )
      c += Outer( "y" )
   CATCH e
      c += " caught " + e:Description
   FINALLY
      c += " finally"
   END
   ? "try:", c
   c := ""
   BEGIN SEQUENCE WITH {| o | Break( { o:Description, ProcName( 1 ), ProcLine( 1 ), ProcName( 2 ) } ) }
      FOR EACH x IN Faulty( 3 )
         c += Str( x, 1 )
      NEXT
   RECOVER USING e
      c += " " + e[ 1 ] + " in " + e[ 2 ] + "(" + LTrim( Str( e[ 3 ] ) ) + ") from " + e[ 4 ]
   END SEQUENCE
   ? "generator error:", c, "x:", x
   c := ""
   BEGIN SEQUENCE WITH {| o | Break( o:Description ) }
      FOR EACH x IN Faulty( 9 )
         c += x
      NEXT
   RECOVER USING e
      c += " " + e
   END SEQUENCE
   ? "body error:", c, "x:", x
   c := ""
   BEGIN SEQUENCE
      FOR EACH x IN Faulty( 9 )
         IF x == 3
            BREAK x * 100
         ENDIF
         c += Str( x, 1 )
      NEXT
   RECOVER USING e
      c += " broke " + LTrim( Str( e ) )
   END SEQUENCE
   ? "break out:", c, "x:", x
   c := ""
   FOR EACH x IN Faulty( 9 )
      BEGIN SEQUENCE WITH {| o | Break( o ) }
         c += IIf( x % 2 == 0, x + "!", Str( x, 1 ) )
      RECOVER
         c += "e"
      END SEQUENCE
   NEXT
   ? "sequence inside:", c, "x:", x, ProcName(), ProcLine() > 0
RETURN

FUNCTION Twice( n )
RETURN n * 2

FUNCTION Outer( s )
RETURN Twice( s ) + "!"

FUNCTION Breaks( n )
   BREAK n
RETURN n

FUNCTION Faulty( nAt )
   LOCAL i
   FOR i := 1 TO 4
      IF i == nAt
         i := i + "x"
      ENDIF
      YIELD i
   NEXT
RETURN
