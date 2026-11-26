// Variables of a function with BEGIN SEQUENCE that the optimizer keeps out of memory: the handler (RECOVER, ALWAYS,
// what follows END) sees the value each had when the operation unwound: a BREAK in the middle of a loop, an error of
// an operator, of a called function and inside a codeblock, nested sequences (the inner one goes on, the outer one
// catches), a variable RECOVER assigns and the code after reads, FINALLY reading the last values, loops after the
// sequence that go on from them, and a FOR EACH left by a BREAK (its cleanup a handler too). Each result is computed
// once by the program and printed.
PROCEDURE Main()
   ? "break in a loop:", BreakAt( 7 ), BreakAt( 100 )
   ? "operator error:", DivideDown( 5 )
   ? "called function:", Called( 4 )
   ? "in a codeblock:", InBlock( 6 )
   ? "nested:", Nested( 3 )
   ? "recover assigns:", Assigned( 9 )
   ? "finally:", Finally( 5 )
   ? "loop after:", After( 4, 10 )
   ? "for each:", EachIn( { 5, 6, 7, 8 }, 7 ), EachIn( { 1, 2 }, 9 )
RETURN

STATIC FUNCTION BreakAt( nAt )
   LOCAL i, nSum := 0, nLast := 0
   BEGIN SEQUENCE
      FOR i := 1 TO 20
         nSum += i
         nLast := i
         IF i == nAt
            Break( i * 100 )
         ENDIF
      NEXT
   RECOVER USING nLast
      nSum := -nSum
   END
RETURN LTrim( Str( nSum ) ) + "/" + LTrim( Str( nLast ) )

STATIC FUNCTION DivideDown( n )
   LOCAL nDone := 0, x := 0
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      DO WHILE .T.
         x += 100 / n
         nDone++
         n--
      ENDDO
   RECOVER
      x := -x
   END
RETURN LTrim( Str( nDone ) ) + "/" + LTrim( Str( x ) ) + "/" + LTrim( Str( n ) )

STATIC FUNCTION Called( n )
   LOCAL i, c := ""
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      FOR i := 1 TO 10
         c += Piece( i, n )
      NEXT
   RECOVER
      c += "!"
   END
RETURN c

STATIC FUNCTION Piece( i, n )
   IF i > n
      RETURN i + "x" // an error: a number plus a string
   ENDIF
RETURN Chr( 64 + i )

STATIC FUNCTION InBlock( nStop )
   LOCAL a := { 1, 2, 3, 4, 5, 6, 7, 8 }, nSeen := 0, nSum := 0
   BEGIN SEQUENCE
      AEval( a, {| x | nSeen++, nSum += x, IIf( x == nStop, Break( x ), NIL ) } )
   END
RETURN LTrim( Str( nSeen ) ) + "/" + LTrim( Str( nSum ) )

STATIC FUNCTION Nested( n )
   LOCAL i, j, nOuter := 0, nInner := 0, cWhere := ""
   BEGIN SEQUENCE
      FOR i := 1 TO n
         BEGIN SEQUENCE
            FOR j := 1 TO 5
               nInner++
               IF j == i + 1
                  Break( j )
               ENDIF
            NEXT
         END
         nOuter++
         cWhere += LTrim( Str( nInner ) ) + ","
      NEXT
      Break( "out" )
   RECOVER USING cWhere
      cWhere += "|" + LTrim( Str( nOuter ) ) + "/" + LTrim( Str( nInner ) )
   END
RETURN cWhere

STATIC FUNCTION Assigned( n )
   LOCAL k := 1, nFound := 0, i
   BEGIN SEQUENCE
      FOR i := 1 TO n
         k *= 2
         IF k > 100
            Break( k )
         ENDIF
      NEXT
   RECOVER USING nFound
      k := 0
   END
   FOR i := 1 TO 3
      k += i
   NEXT
RETURN LTrim( Str( k ) ) + "/" + LTrim( Str( nFound ) )

STATIC FUNCTION Finally( n )
   LOCAL i, nTotal := 0, cSeen := ""
   TRY
      FOR i := 1 TO n
         nTotal += i
         IF i == 3
            Break( "stop" )
         ENDIF
      NEXT
   CATCH
      nTotal += 1000
   FINALLY
      cSeen := LTrim( Str( nTotal ) ) + "@" + LTrim( Str( i ) )
   END
RETURN cSeen

STATIC FUNCTION After( nStop, nTo )
   LOCAL i, s := 0
   BEGIN SEQUENCE
      FOR i := 1 TO nTo
         s += i
         IF i == nStop
            Break( NIL )
         ENDIF
      NEXT
   END
   DO WHILE i < nTo
      i++
      s += i * 10
   ENDDO
RETURN LTrim( Str( s ) ) + "/" + LTrim( Str( i ) )

STATIC FUNCTION EachIn( a, nStop )
   LOCAL x := "before", nSum := 0, nSeen := 0
   BEGIN SEQUENCE
      FOR EACH x IN a
         nSeen++
         nSum += x
         IF x == nStop
            Break( NIL )
         ENDIF
      NEXT
   END
RETURN LTrim( Str( nSum ) ) + "/" + LTrim( Str( nSeen ) ) + "/" + x
