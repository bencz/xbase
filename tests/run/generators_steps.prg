// The steps of generators that FOR EACH runs, and their frames between steps, which compiled code keeps in place
// while the enumerator is local: an old frame whose steps keep new strings in it across their YIELDs, an old
// enumerator's Current read with MoveNext() and Current, and an old loop variable holding an element the enumerator no
// longer does, each alive through a young collection after every step (the strings span the collector's lines, and
// the allocations after it take over what it freed); a step that fails (the error reaches the loop's handler and the
// enumerator has ended); an enumerator another thread reached (shared: its steps through the runtime); a loop
// variable a codeblock captured; MoveNext() and Current beside FOR EACH; more arguments than other slots, and fewer
// arguments than parameters (none, some, skipped ones); generators feeding one another; an endless one left by EXIT.
PROCEDURE Main()
   LOCAL g, x, n, e, t, aBlocks, c, ok

   n := 0
   FOR EACH x IN Keeps( 200 )
      IF ValType( x ) == "C"
         ? x
         EXIT
      ENDIF
      IF x[ 2 ] == 2
         __GcCollect()            // the frame, the enumerator and the loop's reference are old from here on
      ELSEIF x[ 2 ] > 2
         Young()
      ENDIF
      n += x[ 2 ]
   NEXT
   ? "old frame:", n

   g := Fresh( 60 )
   g:MoveNext()
   __GcCollect()
   n := 0
   ok := .T.
   DO WHILE g:MoveNext()
      Young()
      ok := ok .AND. g:Current == Big( ++n + 1 )
   ENDDO
   ? "old current:", ok, n

   g := Fresh( 60 )
   n := 0
   ok := .T.
   FOR EACH x IN g
      IF ++n == 1
         __GcCollect()
      ENDIF
      g:MoveNext()                // the loop variable alone holds its element now
      Young()
      ok := ok .AND. x == Big( 2 * n - 1 )
   NEXT
   ? "old loop variable:", ok, n

   g := Fails()
   n := 0
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      FOR EACH x IN g
         n += x
      NEXT
   RECOVER USING e
      ? "failed step:", n, e:subCode, e:operation, g:MoveNext(), g:Current
   END SEQUENCE

   g := Count( 1, 5 )
   t := ThreadStart( {|| g:MoveNext() } )
   t:Join()
   c := ""
   FOR EACH x IN g
      c += LTrim( Str( x ) )
   NEXT
   ? "shared:", g:Current, c, g:MoveNext()

   aBlocks := {}
   FOR EACH x IN Count( 1, 3 )
      AAdd( aBlocks, {|| x * 10 } )
   NEXT
   ? "captured:", Eval( aBlocks[ 1 ] ), Eval( aBlocks[ 3 ] )

   g := Count( 1, 6 )
   g:MoveNext()
   c := LTrim( Str( g:Current ) )
   FOR EACH x IN g
      c += LTrim( Str( x ) ) + LTrim( Str( g:Current ) )
      IF x == 3
         g:MoveNext()
      ENDIF
   NEXT
   ? "methods:", c

   c := ""
   FOR EACH x IN Args( "a", "b", "c", "d", "e", "f" )
      c += x
   NEXT
   ? "arguments:", c
   ? "fewer arguments:", Joined( Kinds( "a", 2 ) ), Joined( Kinds() ), Joined( Kinds( , , , , , 6 ) )

   n := 0
   FOR EACH x IN Squares( Odd( Count( 1, 20 ) ) )
      n += x
   NEXT
   ? "chained:", n

   n := 0
   FOR EACH x IN Endless()
      IF x > 1000
         EXIT
      ENDIF
      n++
   NEXT
   ? "endless:", n
RETURN

// Each step keeps a new string, a new array and a string only the frame holds, checked after the YIELD.
FUNCTION Keeps( nLast )
   LOCAL i, cKept, aKept, cOnly
   FOR i := 1 TO nLast
      cKept := Big( i )
      cOnly := Replicate( Chr( 65 + i % 26 ), 600 ) + Str( i )
      aKept := { cKept, i }
      YIELD aKept
      IF !( aKept[ 1 ] == Big( i ) .AND. cKept == aKept[ 1 ] .AND. cOnly == Replicate( Chr( 65 + i % 26 ), 600 ) + ;
            Str( i ) )
         YIELD "frame lost at " + LTrim( Str( i ) )
      ENDIF
   NEXT
RETURN

// Each parameter's type, then a local set across the YIELDs: the first step's frame holds only the arguments passed.
FUNCTION Kinds( a, b, c, d, e, f )
   LOCAL x, n := 0
   FOR EACH x IN { a, b, c, d, e, f }
      YIELD ValType( x )
      n++
   NEXT
   YIELD LTrim( Str( n ) )
RETURN

STATIC FUNCTION Joined( g )
   LOCAL c := "", x
   FOR EACH x IN g
      c += x
   NEXT
RETURN c

FUNCTION Fresh( nLast )
   LOCAL k
   FOR k := 1 TO nLast
      YIELD Big( k )
   NEXT
RETURN

// A new string longer than the collector's lines.
STATIC FUNCTION Big( k )
RETURN Replicate( "c", 600 + k % 7 ) + Str( k )

// A young collection, then allocations kept until they take over what it freed.
STATIC PROCEDURE Young()
   LOCAL k, a := {}
   __GcCollect( .T. )
   FOR k := 1 TO 300
      AAdd( a, Replicate( "g", 600 + k % 7 ) + Str( k ) )
   NEXT
RETURN

FUNCTION Fails()
   LOCAL k
   FOR k := 1 TO 5
      YIELD iif( k == 3, k / 0, k )
   NEXT
RETURN

FUNCTION Count( nFrom, nTo )
   LOCAL i
   FOR i := nFrom TO nTo
      YIELD i
   NEXT
RETURN

FUNCTION Args( a, b, c, d, e, f )
   YIELD a + b
   YIELD c + d
   YIELD e + f
RETURN

FUNCTION Odd( g )
   LOCAL x
   FOR EACH x IN g
      IF x % 2 == 1
         YIELD x
      ENDIF
   NEXT
RETURN

FUNCTION Squares( g )
   LOCAL x
   FOR EACH x IN g
      YIELD x * x
   NEXT
RETURN

FUNCTION Endless()
   LOCAL n := 0
   DO WHILE .T.
      YIELD ++n
   ENDDO
RETURN
