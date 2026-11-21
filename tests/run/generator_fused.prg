// Generators fused into the FOR EACH that goes through them (fuse-generators), at -O0 and -O2 (run0.*): the output is
// the one their steps give unfused. A generator with parameters and LOCALs that live across YIELD; one fed by another
// (both fused); several YIELDs (the state between them); one that yields nothing; fewer arguments than parameters;
// a parameter passed by reference that the generator changes; EXIT, LOOP and RETURN in the loop's body; the loop
// variable assigned in the body and restored after the loop; a YIELD inside a FOR EACH over an array and inside nested
// FORs; values the loop carries; an error raised in the generator's body and one in the loop's body (the call stack
// of each); BREAK out of a loop in a sequence; ProcName() in the generator; a codeblock that captures the loop
// variable (not fused: the same output); an enumerator made once and gone through in every pass of a loop around (not
// fused); a fused loop inside another one; the lines the call stack shows.
PROCEDURE Main()
   LOCAL x := "before", n := 0, c := "", r := 10, e
   FOR EACH x IN Range( 1, 5 )
      n += x
   NEXT
   ? "range:", n, "after:", x
   n := 0
   FOR EACH x IN Evens( Range( 1, 20 ) )
      n += x
      c += LTrim( Str( x ) ) + " "
   NEXT
   ? "evens:", n, c
   c := ""
   FOR EACH x IN Several( 3 )
      c += x + " "
   NEXT
   ? "several:", c
   c := "["
   FOR EACH x IN Range( 5, 1 )
      c += "?"
   NEXT
   ? "none:", c + "]", "fewer:", Count( Range( 3 ) )
   FOR EACH x IN Bumps( @r )
      c += LTrim( Str( x ) ) + ","
   NEXT
   ? "by reference:", c, r
   c := ""
   FOR EACH x IN Range( 1, 10 )
      IF x % 2 == 0
         LOOP
      ENDIF
      IF x > 7
         EXIT
      ENDIF
      c += LTrim( Str( x ) )
   NEXT
   ? "exit and loop:", c, x
   ? "return:", FirstOver( 3 ), FirstOver( 99 )
   c := ""
   FOR EACH x IN Range( 1, 3 )
      x := x * 100
      c += LTrim( Str( x ) ) + " "
   NEXT
   ? "assigned:", c, x
   c := ""
   FOR EACH x IN Pairs( { "a", "b" }, 2 )
      c += x + " "
   NEXT
   ? "nested yields:", c
   ? "errors:", Failing( 2 ), Failing( 9 ), BodyFails()
   x := "kept"
   BEGIN SEQUENCE
      FOR EACH x IN Range( 1, 5 )
         IF x == 3
            BREAK x
         ENDIF
      NEXT
   RECOVER USING e
      ? "break:", e, x
   END SEQUENCE
   c := ""
   FOR EACH x IN Named()
      c += x + " "
   NEXT
   ? "procname:", c
   ? "captured:", Captured()
   ? "carried:", Carried( 30 ), Carried( 1 ), Carried( 10 )
   ? "made once, gone through again:", Again( .F. ), Again( .T. )
   Inside()
   ? "lines:", Lines()
RETURN

// An enumerator made once and gone through in every pass of a loop around: not fused (the second pass goes on where
// the first one stopped, or finds it ended).
FUNCTION Again( lExit )
   LOCAL x, k, c := "", g := Range( 1, 5 )
   FOR k := 1 TO 2
      FOR EACH x IN g
         IF lExit .AND. x == 2 .AND. k == 1
            EXIT
         ENDIF
         c += LTrim( Str( x ) )
      NEXT
      c += ";"
   NEXT
RETURN c

// A fused loop inside another one over a generator fed by a generator: the inner loop's variable, saved and restored
// around it, passes the value it carries back to itself.
PROCEDURE Inside()
   LOCAL x, y, c := ""
   FOR EACH x IN Pass( Span( 1, 3 ) )
      FOR EACH y IN Span( x, x + 1 )
         c += LTrim( Str( y ) )
      NEXT
   NEXT
   ? "a loop inside, its variable carried:", c, x, y
RETURN

FUNCTION Span( nFrom, nTo )
   LOCAL i
   FOR i := nFrom TO nTo
      YIELD i
   NEXT
RETURN

FUNCTION Pass( xSource )
   LOCAL x
   FOR EACH x IN xSource
      YIELD x
   NEXT
RETURN

// The lines the call stack shows: the generator's, and the loop's in its caller.
FUNCTION Lines()
   LOCAL c := ""
   BEGIN SEQUENCE WITH {| o | Break( ProcName( 1 ) + "(" + LTrim( Str( ProcLine( 1 ) ) ) + ") " + ;
         ProcName( 2 ) + "(" + LTrim( Str( ProcLine( 2 ) ) ) + ")" ) }
      c := Stepped()
   RECOVER USING c
   END SEQUENCE
RETURN c

FUNCTION Stepped()
   LOCAL x, c := ""
   FOR EACH x IN Failing2()
      c += x
   NEXT
RETURN c

FUNCTION Failing2()
   LOCAL i := 1
   YIELD "a"
   i := i + "x"
   YIELD "b"
RETURN

// Values the loops carry in registers (no sequence keeps them in memory): the inner generator's loop fuses after
// promote-variables made them values its header carries.
FUNCTION Carried( nTo )
   LOCAL x, n := 0, nMax := -1, nCount := 0, c := ""
   FOR EACH x IN Evens( Range( 1, nTo ) )
      n += x
      nCount++
      IF x > nMax
         nMax := x
      ENDIF
      IF nCount > 12
         EXIT
      ENDIF
   NEXT
   FOR EACH x IN Several( 2 )
      c += Left( x, 1 )
   NEXT
RETURN LTrim( Str( n ) ) + "/" + LTrim( Str( nMax ) ) + "/" + LTrim( Str( nCount ) ) + "/" + c

// The loop variable read by a codeblock: shared, so not fused.
FUNCTION Captured()
   LOCAL x, c := "", b := {|| x }
   FOR EACH x IN Range( 7, 9 )
      c += LTrim( Str( Eval( b ) ) )
   NEXT
RETURN c

FUNCTION Range( nFrom, nTo )
   LOCAL i
   IF nTo == NIL
      nTo := nFrom
      nFrom := 1
   ENDIF
   FOR i := nFrom TO nTo
      YIELD i
   NEXT
RETURN

FUNCTION Evens( xSource )
   LOCAL x
   FOR EACH x IN xSource
      IF x % 2 == 0
         YIELD x
      ENDIF
   NEXT
RETURN

FUNCTION Several( n )
   YIELD "one"
   IF n > 1
      YIELD "two"
   ENDIF
   YIELD "three:" + LTrim( Str( n ) )
RETURN

FUNCTION Count( xSource )
   LOCAL x, n := 0
   FOR EACH x IN xSource
      n++
   NEXT
RETURN n

FUNCTION Bumps( nRef )
   LOCAL k
   FOR k := 1 TO 3
      nRef++
      YIELD nRef
   NEXT
RETURN

FUNCTION FirstOver( nLimit )
   LOCAL x
   FOR EACH x IN Range( 1, 10 )
      IF x > nLimit
         RETURN x
      ENDIF
   NEXT
RETURN -1

FUNCTION Pairs( aItems, nTimes )
   LOCAL s, i, j
   FOR EACH s IN aItems
      FOR i := 1 TO nTimes
         FOR j := 1 TO 1
            YIELD s + LTrim( Str( i ) )
         NEXT
      NEXT
   NEXT
RETURN

FUNCTION Failing( nAt )
   LOCAL x, c := ""
   BEGIN SEQUENCE WITH {| o | Break( { o, ProcName( 1 ), ProcName( 2 ) } ) }
      FOR EACH x IN Faulty( nAt )
         c += LTrim( Str( x ) )
      NEXT
   RECOVER USING x
      c += " " + x[ 1 ]:Description + " in " + x[ 2 ] + " from " + x[ 3 ]
   END SEQUENCE
RETURN c

FUNCTION Faulty( nAt )
   LOCAL i
   FOR i := 1 TO 4
      IF i == nAt
         i := i + "x"
      ENDIF
      YIELD i
   NEXT
RETURN

FUNCTION BodyFails()
   LOCAL x, c := ""
   BEGIN SEQUENCE WITH {| o | Break( { o, ProcName( 1 ) } ) }
      FOR EACH x IN Range( 1, 3 )
         c += x
      NEXT
   RECOVER USING x
      c += x[ 1 ]:Description + " in " + x[ 2 ]
   END SEQUENCE
RETURN c

FUNCTION Named()
   YIELD ProcName()
   YIELD ProcName( 1 )
RETURN
