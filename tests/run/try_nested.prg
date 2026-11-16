// TRY ... CATCH ... FINALLY nested at N levels. Lexical nesting: every choice of the levels that have a CATCH up to 4
// levels, and 6 and 8 levels with none, all and every other one (FINALLY at all of them); the request raised at each
// level in four ways: Throw(), a runtime error (its Error object reaches CATCH), every CATCH throwing again, and the
// raising level's FINALLY throwing another value (which replaces the request, caught or not); what no CATCH takes
// reaches the BEGIN SEQUENCE around. Then nesting through recursion, 1 to 60 levels deep, each level's CATCH taking the
// request or throwing it on; and EXIT, LOOP and RETURN from the innermost of N levels, every FINALLY run from the
// inside out. Each case logs the bodies entered (b), the CATCHes run with what they received (c), the FINALLYs (f),
// the statement after each nested TRY (a) and what left the outermost (out); a model written without sequences says
// what the log must be, and a case prints only when they differ.
MEMVAR cLog

PROCEDURE Main()
   LOCAL aShape, nThrow, nMode, nDepth, nPattern, aCatch, aThrows, k, e, nCases := 0, nBad := 0
   PRIVATE cLog
   ErrorBlock( {| x | Break( x ) } )
   FOR EACH aShape IN Shapes()
      FOR nThrow := 0 TO aShape[ 1 ]
         FOR nMode := 0 TO iif( nThrow == 0, 0, 3 )
            cLog := ""
            BEGIN SEQUENCE
               Eval( aShape[ 3 ], nThrow, nMode )
               Mark( "a1" )
            RECOVER USING e
               Mark( "out:" + Shown( e ) )
            END SEQUENCE
            ++nCases
            IF !( SubStr( cLog, 2 ) == Expected( aShape[ 1 ], aShape[ 2 ], nThrow, nMode, NIL ) )
               ++nBad
               ? "lexical", aShape[ 1 ], aShape[ 2 ], nThrow, nMode, "got", SubStr( cLog, 2 )
            ENDIF
         NEXT
      NEXT
   NEXT
   ? "lexical cases:", nCases, "wrong:", nBad
   nCases := nBad := 0
   FOR EACH nDepth IN { 1, 2, 3, 7, 16, 33, 60 }
      aThrows := {}
      FOR EACH nThrow IN { 0, 1, Int( ( nDepth + 1 ) / 2 ), nDepth }
         IF AScan( aThrows, nThrow ) == 0
            AAdd( aThrows, nThrow )
         ENDIF
      NEXT
      FOR EACH nThrow IN aThrows
         FOR nPattern := 1 TO 3
            aCatch := Array( nDepth )
            FOR k := 1 TO nDepth
               aCatch[ k ] := iif( nPattern == 1, .T., iif( nPattern == 2, .F., ( k * 7 + nDepth ) % 10 < 3 ) )
            NEXT
            cLog := ""
            BEGIN SEQUENCE
               Level( 1, nDepth, nThrow, aCatch )
               Mark( "a1" )
            RECOVER USING e
               Mark( "out:" + Shown( e ) )
            END SEQUENCE
            ++nCases
            IF !( SubStr( cLog, 2 ) == Expected( nDepth, 0, nThrow, 0, aCatch ) )
               ++nBad
               ? "recursive", nDepth, nThrow, nPattern, "got", SubStr( cLog, 2 )
            ENDIF
         NEXT
      NEXT
   NEXT
   ? "recursive cases:", nCases, "wrong:", nBad
   Leaving()
RETURN

STATIC PROCEDURE Mark( c )
   cLog += " " + c
RETURN

STATIC FUNCTION N( n )
RETURN LTrim( Str( n ) )

STATIC FUNCTION Shown( x )
RETURN iif( ValType( x ) == "O", "E" + N( x:subCode ), x )

STATIC PROCEDURE Raise( nLevel, nMode )
   IF nMode == 1
      ? 1 + "a"
   ENDIF
   Throw( "t" + N( nLevel ) )
RETURN

// Level k of a recursive nesting: its TRY calls level k + 1; aCatch[ k ] says whether its CATCH takes the request,
// else it throws it on.
STATIC PROCEDURE Level( k, nDepth, nThrow, aCatch )
   LOCAL e
   TRY
      Mark( "b" + N( k ) )
      IF k == nThrow
         Throw( "t" + N( k ) )
      ELSEIF k < nDepth
         Level( k + 1, nDepth, nThrow, aCatch )
         Mark( "a" + N( k + 1 ) )
      ENDIF
   CATCH e
      IF !aCatch[ k ]
         Throw( e )
      ENDIF
      Mark( "c" + N( k ) + ":" + Shown( e ) )
   FINALLY
      Mark( "f" + N( k ) )
   END
RETURN

// The model: the log of n levels whose CATCHes are nMask's bits (or aCatch, which takes every request as it comes)
// when level nThrow raises the request (none with 0) in way nMode.
STATIC FUNCTION Expected( n, nMask, nThrow, nMode, aCatch )
   LOCAL cOut := "", xLeft := Modeled( 1, n, nMask, nThrow, nMode, aCatch, @cOut )
RETURN SubStr( cOut, 2 ) + iif( xLeft == NIL, " a1", " out:" + xLeft )

STATIC FUNCTION Modeled( k, n, nMask, nThrow, nMode, aCatch, cOut )
   LOCAL xPending
   cOut += " b" + N( k )
   IF k == nThrow
      xPending := iif( nMode == 1, "E1081", "t" + N( k ) )
   ELSEIF k < n
      xPending := Modeled( k + 1, n, nMask, nThrow, nMode, aCatch, @cOut )
      IF xPending == NIL
         cOut += " a" + N( k + 1 )
      ENDIF
   ENDIF
   IF xPending != NIL .AND. iif( aCatch == NIL, Int( nMask / 2 ^ ( k - 1 ) ) % 2 == 1, aCatch[ k ] )
      cOut += " c" + N( k ) + ":" + xPending
      xPending := iif( nMode == 2, "r" + N( k ), NIL )
   ENDIF
   cOut += " f" + N( k )
   IF nMode == 3 .AND. k == nThrow
      xPending := "x" + N( k )
   ENDIF
RETURN xPending

// What EXIT, LOOP and RETURN from the innermost of k levels log: LOOP goes round once more.
STATIC FUNCTION LeaveModel( c, k )
   LOCAL cOut := "", i, j
   FOR i := 1 TO iif( c == "LOOP", 2, 1 )
      FOR j := 1 TO k
         cOut += " b" + N( j ) + "." + N( i )
      NEXT
      FOR j := k TO 1 STEP -1
         cOut += " f" + N( j ) + "." + N( i )
      NEXT
   NEXT
RETURN SubStr( cOut, 2 )

// Each shape: its levels, its CATCHes (bit k - 1 for level k) and its function.
STATIC FUNCTION Shapes()
   RETURN { ;
      { 1, 0, {| t, m | L1_0( t, m ) } }, ;
      { 1, 1, {| t, m | L1_1( t, m ) } }, ;
      { 2, 0, {| t, m | L2_0( t, m ) } }, ;
      { 2, 1, {| t, m | L2_1( t, m ) } }, ;
      { 2, 2, {| t, m | L2_2( t, m ) } }, ;
      { 2, 3, {| t, m | L2_3( t, m ) } }, ;
      { 3, 0, {| t, m | L3_0( t, m ) } }, ;
      { 3, 1, {| t, m | L3_1( t, m ) } }, ;
      { 3, 2, {| t, m | L3_2( t, m ) } }, ;
      { 3, 3, {| t, m | L3_3( t, m ) } }, ;
      { 3, 4, {| t, m | L3_4( t, m ) } }, ;
      { 3, 5, {| t, m | L3_5( t, m ) } }, ;
      { 3, 6, {| t, m | L3_6( t, m ) } }, ;
      { 3, 7, {| t, m | L3_7( t, m ) } }, ;
      { 4, 0, {| t, m | L4_0( t, m ) } }, ;
      { 4, 1, {| t, m | L4_1( t, m ) } }, ;
      { 4, 2, {| t, m | L4_2( t, m ) } }, ;
      { 4, 3, {| t, m | L4_3( t, m ) } }, ;
      { 4, 4, {| t, m | L4_4( t, m ) } }, ;
      { 4, 5, {| t, m | L4_5( t, m ) } }, ;
      { 4, 6, {| t, m | L4_6( t, m ) } }, ;
      { 4, 7, {| t, m | L4_7( t, m ) } }, ;
      { 4, 8, {| t, m | L4_8( t, m ) } }, ;
      { 4, 9, {| t, m | L4_9( t, m ) } }, ;
      { 4, 10, {| t, m | L4_10( t, m ) } }, ;
      { 4, 11, {| t, m | L4_11( t, m ) } }, ;
      { 4, 12, {| t, m | L4_12( t, m ) } }, ;
      { 4, 13, {| t, m | L4_13( t, m ) } }, ;
      { 4, 14, {| t, m | L4_14( t, m ) } }, ;
      { 4, 15, {| t, m | L4_15( t, m ) } }, ;
      { 6, 0, {| t, m | L6_0( t, m ) } }, ;
      { 6, 63, {| t, m | L6_63( t, m ) } }, ;
      { 6, 21, {| t, m | L6_21( t, m ) } }, ;
      { 6, 42, {| t, m | L6_42( t, m ) } }, ;
      { 8, 0, {| t, m | L8_0( t, m ) } }, ;
      { 8, 255, {| t, m | L8_255( t, m ) } }, ;
      { 8, 85, {| t, m | L8_85( t, m ) } }, ;
      { 8, 170, {| t, m | L8_170( t, m ) } } }

STATIC PROCEDURE L1_0( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L1_1( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ENDIF
   CATCH e
      Mark( "c1:" + Shown( e ) )
      IF nMode == 2
         Throw( "r1" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L2_0( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L2_1( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   CATCH e
      Mark( "c1:" + Shown( e ) )
      IF nMode == 2
         Throw( "r1" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L2_2( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ENDIF
         CATCH e
            Mark( "c2:" + Shown( e ) )
            IF nMode == 2
               Throw( "r2" )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L2_3( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ENDIF
         CATCH e
            Mark( "c2:" + Shown( e ) )
            IF nMode == 2
               Throw( "r2" )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   CATCH e
      Mark( "c1:" + Shown( e ) )
      IF nMode == 2
         Throw( "r1" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L3_0( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ELSE
               TRY
                  Mark( "b3" )
                  IF nThrow == 3
                     Raise( 3, nMode )
                  ENDIF
               FINALLY
                  Mark( "f3" )
                  IF nMode == 3 .AND. nThrow == 3
                     Throw( "x3" )
                  ENDIF
               END
               Mark( "a3" )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L3_1( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ELSE
               TRY
                  Mark( "b3" )
                  IF nThrow == 3
                     Raise( 3, nMode )
                  ENDIF
               FINALLY
                  Mark( "f3" )
                  IF nMode == 3 .AND. nThrow == 3
                     Throw( "x3" )
                  ENDIF
               END
               Mark( "a3" )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   CATCH e
      Mark( "c1:" + Shown( e ) )
      IF nMode == 2
         Throw( "r1" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L3_2( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ELSE
               TRY
                  Mark( "b3" )
                  IF nThrow == 3
                     Raise( 3, nMode )
                  ENDIF
               FINALLY
                  Mark( "f3" )
                  IF nMode == 3 .AND. nThrow == 3
                     Throw( "x3" )
                  ENDIF
               END
               Mark( "a3" )
            ENDIF
         CATCH e
            Mark( "c2:" + Shown( e ) )
            IF nMode == 2
               Throw( "r2" )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L3_3( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ELSE
               TRY
                  Mark( "b3" )
                  IF nThrow == 3
                     Raise( 3, nMode )
                  ENDIF
               FINALLY
                  Mark( "f3" )
                  IF nMode == 3 .AND. nThrow == 3
                     Throw( "x3" )
                  ENDIF
               END
               Mark( "a3" )
            ENDIF
         CATCH e
            Mark( "c2:" + Shown( e ) )
            IF nMode == 2
               Throw( "r2" )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   CATCH e
      Mark( "c1:" + Shown( e ) )
      IF nMode == 2
         Throw( "r1" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L3_4( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ELSE
               TRY
                  Mark( "b3" )
                  IF nThrow == 3
                     Raise( 3, nMode )
                  ENDIF
               CATCH e
                  Mark( "c3:" + Shown( e ) )
                  IF nMode == 2
                     Throw( "r3" )
                  ENDIF
               FINALLY
                  Mark( "f3" )
                  IF nMode == 3 .AND. nThrow == 3
                     Throw( "x3" )
                  ENDIF
               END
               Mark( "a3" )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L3_5( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ELSE
               TRY
                  Mark( "b3" )
                  IF nThrow == 3
                     Raise( 3, nMode )
                  ENDIF
               CATCH e
                  Mark( "c3:" + Shown( e ) )
                  IF nMode == 2
                     Throw( "r3" )
                  ENDIF
               FINALLY
                  Mark( "f3" )
                  IF nMode == 3 .AND. nThrow == 3
                     Throw( "x3" )
                  ENDIF
               END
               Mark( "a3" )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   CATCH e
      Mark( "c1:" + Shown( e ) )
      IF nMode == 2
         Throw( "r1" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L3_6( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ELSE
               TRY
                  Mark( "b3" )
                  IF nThrow == 3
                     Raise( 3, nMode )
                  ENDIF
               CATCH e
                  Mark( "c3:" + Shown( e ) )
                  IF nMode == 2
                     Throw( "r3" )
                  ENDIF
               FINALLY
                  Mark( "f3" )
                  IF nMode == 3 .AND. nThrow == 3
                     Throw( "x3" )
                  ENDIF
               END
               Mark( "a3" )
            ENDIF
         CATCH e
            Mark( "c2:" + Shown( e ) )
            IF nMode == 2
               Throw( "r2" )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L3_7( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ELSE
               TRY
                  Mark( "b3" )
                  IF nThrow == 3
                     Raise( 3, nMode )
                  ENDIF
               CATCH e
                  Mark( "c3:" + Shown( e ) )
                  IF nMode == 2
                     Throw( "r3" )
                  ENDIF
               FINALLY
                  Mark( "f3" )
                  IF nMode == 3 .AND. nThrow == 3
                     Throw( "x3" )
                  ENDIF
               END
               Mark( "a3" )
            ENDIF
         CATCH e
            Mark( "c2:" + Shown( e ) )
            IF nMode == 2
               Throw( "r2" )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   CATCH e
      Mark( "c1:" + Shown( e ) )
      IF nMode == 2
         Throw( "r1" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L4_0( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ELSE
               TRY
                  Mark( "b3" )
                  IF nThrow == 3
                     Raise( 3, nMode )
                  ELSE
                     TRY
                        Mark( "b4" )
                        IF nThrow == 4
                           Raise( 4, nMode )
                        ENDIF
                     FINALLY
                        Mark( "f4" )
                        IF nMode == 3 .AND. nThrow == 4
                           Throw( "x4" )
                        ENDIF
                     END
                     Mark( "a4" )
                  ENDIF
               FINALLY
                  Mark( "f3" )
                  IF nMode == 3 .AND. nThrow == 3
                     Throw( "x3" )
                  ENDIF
               END
               Mark( "a3" )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L4_1( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ELSE
               TRY
                  Mark( "b3" )
                  IF nThrow == 3
                     Raise( 3, nMode )
                  ELSE
                     TRY
                        Mark( "b4" )
                        IF nThrow == 4
                           Raise( 4, nMode )
                        ENDIF
                     FINALLY
                        Mark( "f4" )
                        IF nMode == 3 .AND. nThrow == 4
                           Throw( "x4" )
                        ENDIF
                     END
                     Mark( "a4" )
                  ENDIF
               FINALLY
                  Mark( "f3" )
                  IF nMode == 3 .AND. nThrow == 3
                     Throw( "x3" )
                  ENDIF
               END
               Mark( "a3" )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   CATCH e
      Mark( "c1:" + Shown( e ) )
      IF nMode == 2
         Throw( "r1" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L4_2( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ELSE
               TRY
                  Mark( "b3" )
                  IF nThrow == 3
                     Raise( 3, nMode )
                  ELSE
                     TRY
                        Mark( "b4" )
                        IF nThrow == 4
                           Raise( 4, nMode )
                        ENDIF
                     FINALLY
                        Mark( "f4" )
                        IF nMode == 3 .AND. nThrow == 4
                           Throw( "x4" )
                        ENDIF
                     END
                     Mark( "a4" )
                  ENDIF
               FINALLY
                  Mark( "f3" )
                  IF nMode == 3 .AND. nThrow == 3
                     Throw( "x3" )
                  ENDIF
               END
               Mark( "a3" )
            ENDIF
         CATCH e
            Mark( "c2:" + Shown( e ) )
            IF nMode == 2
               Throw( "r2" )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L4_3( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ELSE
               TRY
                  Mark( "b3" )
                  IF nThrow == 3
                     Raise( 3, nMode )
                  ELSE
                     TRY
                        Mark( "b4" )
                        IF nThrow == 4
                           Raise( 4, nMode )
                        ENDIF
                     FINALLY
                        Mark( "f4" )
                        IF nMode == 3 .AND. nThrow == 4
                           Throw( "x4" )
                        ENDIF
                     END
                     Mark( "a4" )
                  ENDIF
               FINALLY
                  Mark( "f3" )
                  IF nMode == 3 .AND. nThrow == 3
                     Throw( "x3" )
                  ENDIF
               END
               Mark( "a3" )
            ENDIF
         CATCH e
            Mark( "c2:" + Shown( e ) )
            IF nMode == 2
               Throw( "r2" )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   CATCH e
      Mark( "c1:" + Shown( e ) )
      IF nMode == 2
         Throw( "r1" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L4_4( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ELSE
               TRY
                  Mark( "b3" )
                  IF nThrow == 3
                     Raise( 3, nMode )
                  ELSE
                     TRY
                        Mark( "b4" )
                        IF nThrow == 4
                           Raise( 4, nMode )
                        ENDIF
                     FINALLY
                        Mark( "f4" )
                        IF nMode == 3 .AND. nThrow == 4
                           Throw( "x4" )
                        ENDIF
                     END
                     Mark( "a4" )
                  ENDIF
               CATCH e
                  Mark( "c3:" + Shown( e ) )
                  IF nMode == 2
                     Throw( "r3" )
                  ENDIF
               FINALLY
                  Mark( "f3" )
                  IF nMode == 3 .AND. nThrow == 3
                     Throw( "x3" )
                  ENDIF
               END
               Mark( "a3" )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L4_5( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ELSE
               TRY
                  Mark( "b3" )
                  IF nThrow == 3
                     Raise( 3, nMode )
                  ELSE
                     TRY
                        Mark( "b4" )
                        IF nThrow == 4
                           Raise( 4, nMode )
                        ENDIF
                     FINALLY
                        Mark( "f4" )
                        IF nMode == 3 .AND. nThrow == 4
                           Throw( "x4" )
                        ENDIF
                     END
                     Mark( "a4" )
                  ENDIF
               CATCH e
                  Mark( "c3:" + Shown( e ) )
                  IF nMode == 2
                     Throw( "r3" )
                  ENDIF
               FINALLY
                  Mark( "f3" )
                  IF nMode == 3 .AND. nThrow == 3
                     Throw( "x3" )
                  ENDIF
               END
               Mark( "a3" )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   CATCH e
      Mark( "c1:" + Shown( e ) )
      IF nMode == 2
         Throw( "r1" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L4_6( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ELSE
               TRY
                  Mark( "b3" )
                  IF nThrow == 3
                     Raise( 3, nMode )
                  ELSE
                     TRY
                        Mark( "b4" )
                        IF nThrow == 4
                           Raise( 4, nMode )
                        ENDIF
                     FINALLY
                        Mark( "f4" )
                        IF nMode == 3 .AND. nThrow == 4
                           Throw( "x4" )
                        ENDIF
                     END
                     Mark( "a4" )
                  ENDIF
               CATCH e
                  Mark( "c3:" + Shown( e ) )
                  IF nMode == 2
                     Throw( "r3" )
                  ENDIF
               FINALLY
                  Mark( "f3" )
                  IF nMode == 3 .AND. nThrow == 3
                     Throw( "x3" )
                  ENDIF
               END
               Mark( "a3" )
            ENDIF
         CATCH e
            Mark( "c2:" + Shown( e ) )
            IF nMode == 2
               Throw( "r2" )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L4_7( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ELSE
               TRY
                  Mark( "b3" )
                  IF nThrow == 3
                     Raise( 3, nMode )
                  ELSE
                     TRY
                        Mark( "b4" )
                        IF nThrow == 4
                           Raise( 4, nMode )
                        ENDIF
                     FINALLY
                        Mark( "f4" )
                        IF nMode == 3 .AND. nThrow == 4
                           Throw( "x4" )
                        ENDIF
                     END
                     Mark( "a4" )
                  ENDIF
               CATCH e
                  Mark( "c3:" + Shown( e ) )
                  IF nMode == 2
                     Throw( "r3" )
                  ENDIF
               FINALLY
                  Mark( "f3" )
                  IF nMode == 3 .AND. nThrow == 3
                     Throw( "x3" )
                  ENDIF
               END
               Mark( "a3" )
            ENDIF
         CATCH e
            Mark( "c2:" + Shown( e ) )
            IF nMode == 2
               Throw( "r2" )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   CATCH e
      Mark( "c1:" + Shown( e ) )
      IF nMode == 2
         Throw( "r1" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L4_8( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ELSE
               TRY
                  Mark( "b3" )
                  IF nThrow == 3
                     Raise( 3, nMode )
                  ELSE
                     TRY
                        Mark( "b4" )
                        IF nThrow == 4
                           Raise( 4, nMode )
                        ENDIF
                     CATCH e
                        Mark( "c4:" + Shown( e ) )
                        IF nMode == 2
                           Throw( "r4" )
                        ENDIF
                     FINALLY
                        Mark( "f4" )
                        IF nMode == 3 .AND. nThrow == 4
                           Throw( "x4" )
                        ENDIF
                     END
                     Mark( "a4" )
                  ENDIF
               FINALLY
                  Mark( "f3" )
                  IF nMode == 3 .AND. nThrow == 3
                     Throw( "x3" )
                  ENDIF
               END
               Mark( "a3" )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L4_9( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ELSE
               TRY
                  Mark( "b3" )
                  IF nThrow == 3
                     Raise( 3, nMode )
                  ELSE
                     TRY
                        Mark( "b4" )
                        IF nThrow == 4
                           Raise( 4, nMode )
                        ENDIF
                     CATCH e
                        Mark( "c4:" + Shown( e ) )
                        IF nMode == 2
                           Throw( "r4" )
                        ENDIF
                     FINALLY
                        Mark( "f4" )
                        IF nMode == 3 .AND. nThrow == 4
                           Throw( "x4" )
                        ENDIF
                     END
                     Mark( "a4" )
                  ENDIF
               FINALLY
                  Mark( "f3" )
                  IF nMode == 3 .AND. nThrow == 3
                     Throw( "x3" )
                  ENDIF
               END
               Mark( "a3" )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   CATCH e
      Mark( "c1:" + Shown( e ) )
      IF nMode == 2
         Throw( "r1" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L4_10( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ELSE
               TRY
                  Mark( "b3" )
                  IF nThrow == 3
                     Raise( 3, nMode )
                  ELSE
                     TRY
                        Mark( "b4" )
                        IF nThrow == 4
                           Raise( 4, nMode )
                        ENDIF
                     CATCH e
                        Mark( "c4:" + Shown( e ) )
                        IF nMode == 2
                           Throw( "r4" )
                        ENDIF
                     FINALLY
                        Mark( "f4" )
                        IF nMode == 3 .AND. nThrow == 4
                           Throw( "x4" )
                        ENDIF
                     END
                     Mark( "a4" )
                  ENDIF
               FINALLY
                  Mark( "f3" )
                  IF nMode == 3 .AND. nThrow == 3
                     Throw( "x3" )
                  ENDIF
               END
               Mark( "a3" )
            ENDIF
         CATCH e
            Mark( "c2:" + Shown( e ) )
            IF nMode == 2
               Throw( "r2" )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L4_11( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ELSE
               TRY
                  Mark( "b3" )
                  IF nThrow == 3
                     Raise( 3, nMode )
                  ELSE
                     TRY
                        Mark( "b4" )
                        IF nThrow == 4
                           Raise( 4, nMode )
                        ENDIF
                     CATCH e
                        Mark( "c4:" + Shown( e ) )
                        IF nMode == 2
                           Throw( "r4" )
                        ENDIF
                     FINALLY
                        Mark( "f4" )
                        IF nMode == 3 .AND. nThrow == 4
                           Throw( "x4" )
                        ENDIF
                     END
                     Mark( "a4" )
                  ENDIF
               FINALLY
                  Mark( "f3" )
                  IF nMode == 3 .AND. nThrow == 3
                     Throw( "x3" )
                  ENDIF
               END
               Mark( "a3" )
            ENDIF
         CATCH e
            Mark( "c2:" + Shown( e ) )
            IF nMode == 2
               Throw( "r2" )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   CATCH e
      Mark( "c1:" + Shown( e ) )
      IF nMode == 2
         Throw( "r1" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L4_12( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ELSE
               TRY
                  Mark( "b3" )
                  IF nThrow == 3
                     Raise( 3, nMode )
                  ELSE
                     TRY
                        Mark( "b4" )
                        IF nThrow == 4
                           Raise( 4, nMode )
                        ENDIF
                     CATCH e
                        Mark( "c4:" + Shown( e ) )
                        IF nMode == 2
                           Throw( "r4" )
                        ENDIF
                     FINALLY
                        Mark( "f4" )
                        IF nMode == 3 .AND. nThrow == 4
                           Throw( "x4" )
                        ENDIF
                     END
                     Mark( "a4" )
                  ENDIF
               CATCH e
                  Mark( "c3:" + Shown( e ) )
                  IF nMode == 2
                     Throw( "r3" )
                  ENDIF
               FINALLY
                  Mark( "f3" )
                  IF nMode == 3 .AND. nThrow == 3
                     Throw( "x3" )
                  ENDIF
               END
               Mark( "a3" )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L4_13( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ELSE
               TRY
                  Mark( "b3" )
                  IF nThrow == 3
                     Raise( 3, nMode )
                  ELSE
                     TRY
                        Mark( "b4" )
                        IF nThrow == 4
                           Raise( 4, nMode )
                        ENDIF
                     CATCH e
                        Mark( "c4:" + Shown( e ) )
                        IF nMode == 2
                           Throw( "r4" )
                        ENDIF
                     FINALLY
                        Mark( "f4" )
                        IF nMode == 3 .AND. nThrow == 4
                           Throw( "x4" )
                        ENDIF
                     END
                     Mark( "a4" )
                  ENDIF
               CATCH e
                  Mark( "c3:" + Shown( e ) )
                  IF nMode == 2
                     Throw( "r3" )
                  ENDIF
               FINALLY
                  Mark( "f3" )
                  IF nMode == 3 .AND. nThrow == 3
                     Throw( "x3" )
                  ENDIF
               END
               Mark( "a3" )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   CATCH e
      Mark( "c1:" + Shown( e ) )
      IF nMode == 2
         Throw( "r1" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L4_14( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ELSE
               TRY
                  Mark( "b3" )
                  IF nThrow == 3
                     Raise( 3, nMode )
                  ELSE
                     TRY
                        Mark( "b4" )
                        IF nThrow == 4
                           Raise( 4, nMode )
                        ENDIF
                     CATCH e
                        Mark( "c4:" + Shown( e ) )
                        IF nMode == 2
                           Throw( "r4" )
                        ENDIF
                     FINALLY
                        Mark( "f4" )
                        IF nMode == 3 .AND. nThrow == 4
                           Throw( "x4" )
                        ENDIF
                     END
                     Mark( "a4" )
                  ENDIF
               CATCH e
                  Mark( "c3:" + Shown( e ) )
                  IF nMode == 2
                     Throw( "r3" )
                  ENDIF
               FINALLY
                  Mark( "f3" )
                  IF nMode == 3 .AND. nThrow == 3
                     Throw( "x3" )
                  ENDIF
               END
               Mark( "a3" )
            ENDIF
         CATCH e
            Mark( "c2:" + Shown( e ) )
            IF nMode == 2
               Throw( "r2" )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L4_15( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ELSE
               TRY
                  Mark( "b3" )
                  IF nThrow == 3
                     Raise( 3, nMode )
                  ELSE
                     TRY
                        Mark( "b4" )
                        IF nThrow == 4
                           Raise( 4, nMode )
                        ENDIF
                     CATCH e
                        Mark( "c4:" + Shown( e ) )
                        IF nMode == 2
                           Throw( "r4" )
                        ENDIF
                     FINALLY
                        Mark( "f4" )
                        IF nMode == 3 .AND. nThrow == 4
                           Throw( "x4" )
                        ENDIF
                     END
                     Mark( "a4" )
                  ENDIF
               CATCH e
                  Mark( "c3:" + Shown( e ) )
                  IF nMode == 2
                     Throw( "r3" )
                  ENDIF
               FINALLY
                  Mark( "f3" )
                  IF nMode == 3 .AND. nThrow == 3
                     Throw( "x3" )
                  ENDIF
               END
               Mark( "a3" )
            ENDIF
         CATCH e
            Mark( "c2:" + Shown( e ) )
            IF nMode == 2
               Throw( "r2" )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   CATCH e
      Mark( "c1:" + Shown( e ) )
      IF nMode == 2
         Throw( "r1" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L6_0( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ELSE
               TRY
                  Mark( "b3" )
                  IF nThrow == 3
                     Raise( 3, nMode )
                  ELSE
                     TRY
                        Mark( "b4" )
                        IF nThrow == 4
                           Raise( 4, nMode )
                        ELSE
                           TRY
                              Mark( "b5" )
                              IF nThrow == 5
                                 Raise( 5, nMode )
                              ELSE
                                 TRY
                                    Mark( "b6" )
                                    IF nThrow == 6
                                       Raise( 6, nMode )
                                    ENDIF
                                 FINALLY
                                    Mark( "f6" )
                                    IF nMode == 3 .AND. nThrow == 6
                                       Throw( "x6" )
                                    ENDIF
                                 END
                                 Mark( "a6" )
                              ENDIF
                           FINALLY
                              Mark( "f5" )
                              IF nMode == 3 .AND. nThrow == 5
                                 Throw( "x5" )
                              ENDIF
                           END
                           Mark( "a5" )
                        ENDIF
                     FINALLY
                        Mark( "f4" )
                        IF nMode == 3 .AND. nThrow == 4
                           Throw( "x4" )
                        ENDIF
                     END
                     Mark( "a4" )
                  ENDIF
               FINALLY
                  Mark( "f3" )
                  IF nMode == 3 .AND. nThrow == 3
                     Throw( "x3" )
                  ENDIF
               END
               Mark( "a3" )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L6_63( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ELSE
               TRY
                  Mark( "b3" )
                  IF nThrow == 3
                     Raise( 3, nMode )
                  ELSE
                     TRY
                        Mark( "b4" )
                        IF nThrow == 4
                           Raise( 4, nMode )
                        ELSE
                           TRY
                              Mark( "b5" )
                              IF nThrow == 5
                                 Raise( 5, nMode )
                              ELSE
                                 TRY
                                    Mark( "b6" )
                                    IF nThrow == 6
                                       Raise( 6, nMode )
                                    ENDIF
                                 CATCH e
                                    Mark( "c6:" + Shown( e ) )
                                    IF nMode == 2
                                       Throw( "r6" )
                                    ENDIF
                                 FINALLY
                                    Mark( "f6" )
                                    IF nMode == 3 .AND. nThrow == 6
                                       Throw( "x6" )
                                    ENDIF
                                 END
                                 Mark( "a6" )
                              ENDIF
                           CATCH e
                              Mark( "c5:" + Shown( e ) )
                              IF nMode == 2
                                 Throw( "r5" )
                              ENDIF
                           FINALLY
                              Mark( "f5" )
                              IF nMode == 3 .AND. nThrow == 5
                                 Throw( "x5" )
                              ENDIF
                           END
                           Mark( "a5" )
                        ENDIF
                     CATCH e
                        Mark( "c4:" + Shown( e ) )
                        IF nMode == 2
                           Throw( "r4" )
                        ENDIF
                     FINALLY
                        Mark( "f4" )
                        IF nMode == 3 .AND. nThrow == 4
                           Throw( "x4" )
                        ENDIF
                     END
                     Mark( "a4" )
                  ENDIF
               CATCH e
                  Mark( "c3:" + Shown( e ) )
                  IF nMode == 2
                     Throw( "r3" )
                  ENDIF
               FINALLY
                  Mark( "f3" )
                  IF nMode == 3 .AND. nThrow == 3
                     Throw( "x3" )
                  ENDIF
               END
               Mark( "a3" )
            ENDIF
         CATCH e
            Mark( "c2:" + Shown( e ) )
            IF nMode == 2
               Throw( "r2" )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   CATCH e
      Mark( "c1:" + Shown( e ) )
      IF nMode == 2
         Throw( "r1" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L6_21( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ELSE
               TRY
                  Mark( "b3" )
                  IF nThrow == 3
                     Raise( 3, nMode )
                  ELSE
                     TRY
                        Mark( "b4" )
                        IF nThrow == 4
                           Raise( 4, nMode )
                        ELSE
                           TRY
                              Mark( "b5" )
                              IF nThrow == 5
                                 Raise( 5, nMode )
                              ELSE
                                 TRY
                                    Mark( "b6" )
                                    IF nThrow == 6
                                       Raise( 6, nMode )
                                    ENDIF
                                 FINALLY
                                    Mark( "f6" )
                                    IF nMode == 3 .AND. nThrow == 6
                                       Throw( "x6" )
                                    ENDIF
                                 END
                                 Mark( "a6" )
                              ENDIF
                           CATCH e
                              Mark( "c5:" + Shown( e ) )
                              IF nMode == 2
                                 Throw( "r5" )
                              ENDIF
                           FINALLY
                              Mark( "f5" )
                              IF nMode == 3 .AND. nThrow == 5
                                 Throw( "x5" )
                              ENDIF
                           END
                           Mark( "a5" )
                        ENDIF
                     FINALLY
                        Mark( "f4" )
                        IF nMode == 3 .AND. nThrow == 4
                           Throw( "x4" )
                        ENDIF
                     END
                     Mark( "a4" )
                  ENDIF
               CATCH e
                  Mark( "c3:" + Shown( e ) )
                  IF nMode == 2
                     Throw( "r3" )
                  ENDIF
               FINALLY
                  Mark( "f3" )
                  IF nMode == 3 .AND. nThrow == 3
                     Throw( "x3" )
                  ENDIF
               END
               Mark( "a3" )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   CATCH e
      Mark( "c1:" + Shown( e ) )
      IF nMode == 2
         Throw( "r1" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L6_42( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ELSE
               TRY
                  Mark( "b3" )
                  IF nThrow == 3
                     Raise( 3, nMode )
                  ELSE
                     TRY
                        Mark( "b4" )
                        IF nThrow == 4
                           Raise( 4, nMode )
                        ELSE
                           TRY
                              Mark( "b5" )
                              IF nThrow == 5
                                 Raise( 5, nMode )
                              ELSE
                                 TRY
                                    Mark( "b6" )
                                    IF nThrow == 6
                                       Raise( 6, nMode )
                                    ENDIF
                                 CATCH e
                                    Mark( "c6:" + Shown( e ) )
                                    IF nMode == 2
                                       Throw( "r6" )
                                    ENDIF
                                 FINALLY
                                    Mark( "f6" )
                                    IF nMode == 3 .AND. nThrow == 6
                                       Throw( "x6" )
                                    ENDIF
                                 END
                                 Mark( "a6" )
                              ENDIF
                           FINALLY
                              Mark( "f5" )
                              IF nMode == 3 .AND. nThrow == 5
                                 Throw( "x5" )
                              ENDIF
                           END
                           Mark( "a5" )
                        ENDIF
                     CATCH e
                        Mark( "c4:" + Shown( e ) )
                        IF nMode == 2
                           Throw( "r4" )
                        ENDIF
                     FINALLY
                        Mark( "f4" )
                        IF nMode == 3 .AND. nThrow == 4
                           Throw( "x4" )
                        ENDIF
                     END
                     Mark( "a4" )
                  ENDIF
               FINALLY
                  Mark( "f3" )
                  IF nMode == 3 .AND. nThrow == 3
                     Throw( "x3" )
                  ENDIF
               END
               Mark( "a3" )
            ENDIF
         CATCH e
            Mark( "c2:" + Shown( e ) )
            IF nMode == 2
               Throw( "r2" )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L8_0( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ELSE
               TRY
                  Mark( "b3" )
                  IF nThrow == 3
                     Raise( 3, nMode )
                  ELSE
                     TRY
                        Mark( "b4" )
                        IF nThrow == 4
                           Raise( 4, nMode )
                        ELSE
                           TRY
                              Mark( "b5" )
                              IF nThrow == 5
                                 Raise( 5, nMode )
                              ELSE
                                 TRY
                                    Mark( "b6" )
                                    IF nThrow == 6
                                       Raise( 6, nMode )
                                    ELSE
                                       TRY
                                          Mark( "b7" )
                                          IF nThrow == 7
                                             Raise( 7, nMode )
                                          ELSE
                                             TRY
                                                Mark( "b8" )
                                                IF nThrow == 8
                                                   Raise( 8, nMode )
                                                ENDIF
                                             FINALLY
                                                Mark( "f8" )
                                                IF nMode == 3 .AND. nThrow == 8
                                                   Throw( "x8" )
                                                ENDIF
                                             END
                                             Mark( "a8" )
                                          ENDIF
                                       FINALLY
                                          Mark( "f7" )
                                          IF nMode == 3 .AND. nThrow == 7
                                             Throw( "x7" )
                                          ENDIF
                                       END
                                       Mark( "a7" )
                                    ENDIF
                                 FINALLY
                                    Mark( "f6" )
                                    IF nMode == 3 .AND. nThrow == 6
                                       Throw( "x6" )
                                    ENDIF
                                 END
                                 Mark( "a6" )
                              ENDIF
                           FINALLY
                              Mark( "f5" )
                              IF nMode == 3 .AND. nThrow == 5
                                 Throw( "x5" )
                              ENDIF
                           END
                           Mark( "a5" )
                        ENDIF
                     FINALLY
                        Mark( "f4" )
                        IF nMode == 3 .AND. nThrow == 4
                           Throw( "x4" )
                        ENDIF
                     END
                     Mark( "a4" )
                  ENDIF
               FINALLY
                  Mark( "f3" )
                  IF nMode == 3 .AND. nThrow == 3
                     Throw( "x3" )
                  ENDIF
               END
               Mark( "a3" )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L8_255( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ELSE
               TRY
                  Mark( "b3" )
                  IF nThrow == 3
                     Raise( 3, nMode )
                  ELSE
                     TRY
                        Mark( "b4" )
                        IF nThrow == 4
                           Raise( 4, nMode )
                        ELSE
                           TRY
                              Mark( "b5" )
                              IF nThrow == 5
                                 Raise( 5, nMode )
                              ELSE
                                 TRY
                                    Mark( "b6" )
                                    IF nThrow == 6
                                       Raise( 6, nMode )
                                    ELSE
                                       TRY
                                          Mark( "b7" )
                                          IF nThrow == 7
                                             Raise( 7, nMode )
                                          ELSE
                                             TRY
                                                Mark( "b8" )
                                                IF nThrow == 8
                                                   Raise( 8, nMode )
                                                ENDIF
                                             CATCH e
                                                Mark( "c8:" + Shown( e ) )
                                                IF nMode == 2
                                                   Throw( "r8" )
                                                ENDIF
                                             FINALLY
                                                Mark( "f8" )
                                                IF nMode == 3 .AND. nThrow == 8
                                                   Throw( "x8" )
                                                ENDIF
                                             END
                                             Mark( "a8" )
                                          ENDIF
                                       CATCH e
                                          Mark( "c7:" + Shown( e ) )
                                          IF nMode == 2
                                             Throw( "r7" )
                                          ENDIF
                                       FINALLY
                                          Mark( "f7" )
                                          IF nMode == 3 .AND. nThrow == 7
                                             Throw( "x7" )
                                          ENDIF
                                       END
                                       Mark( "a7" )
                                    ENDIF
                                 CATCH e
                                    Mark( "c6:" + Shown( e ) )
                                    IF nMode == 2
                                       Throw( "r6" )
                                    ENDIF
                                 FINALLY
                                    Mark( "f6" )
                                    IF nMode == 3 .AND. nThrow == 6
                                       Throw( "x6" )
                                    ENDIF
                                 END
                                 Mark( "a6" )
                              ENDIF
                           CATCH e
                              Mark( "c5:" + Shown( e ) )
                              IF nMode == 2
                                 Throw( "r5" )
                              ENDIF
                           FINALLY
                              Mark( "f5" )
                              IF nMode == 3 .AND. nThrow == 5
                                 Throw( "x5" )
                              ENDIF
                           END
                           Mark( "a5" )
                        ENDIF
                     CATCH e
                        Mark( "c4:" + Shown( e ) )
                        IF nMode == 2
                           Throw( "r4" )
                        ENDIF
                     FINALLY
                        Mark( "f4" )
                        IF nMode == 3 .AND. nThrow == 4
                           Throw( "x4" )
                        ENDIF
                     END
                     Mark( "a4" )
                  ENDIF
               CATCH e
                  Mark( "c3:" + Shown( e ) )
                  IF nMode == 2
                     Throw( "r3" )
                  ENDIF
               FINALLY
                  Mark( "f3" )
                  IF nMode == 3 .AND. nThrow == 3
                     Throw( "x3" )
                  ENDIF
               END
               Mark( "a3" )
            ENDIF
         CATCH e
            Mark( "c2:" + Shown( e ) )
            IF nMode == 2
               Throw( "r2" )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   CATCH e
      Mark( "c1:" + Shown( e ) )
      IF nMode == 2
         Throw( "r1" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L8_85( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ELSE
               TRY
                  Mark( "b3" )
                  IF nThrow == 3
                     Raise( 3, nMode )
                  ELSE
                     TRY
                        Mark( "b4" )
                        IF nThrow == 4
                           Raise( 4, nMode )
                        ELSE
                           TRY
                              Mark( "b5" )
                              IF nThrow == 5
                                 Raise( 5, nMode )
                              ELSE
                                 TRY
                                    Mark( "b6" )
                                    IF nThrow == 6
                                       Raise( 6, nMode )
                                    ELSE
                                       TRY
                                          Mark( "b7" )
                                          IF nThrow == 7
                                             Raise( 7, nMode )
                                          ELSE
                                             TRY
                                                Mark( "b8" )
                                                IF nThrow == 8
                                                   Raise( 8, nMode )
                                                ENDIF
                                             FINALLY
                                                Mark( "f8" )
                                                IF nMode == 3 .AND. nThrow == 8
                                                   Throw( "x8" )
                                                ENDIF
                                             END
                                             Mark( "a8" )
                                          ENDIF
                                       CATCH e
                                          Mark( "c7:" + Shown( e ) )
                                          IF nMode == 2
                                             Throw( "r7" )
                                          ENDIF
                                       FINALLY
                                          Mark( "f7" )
                                          IF nMode == 3 .AND. nThrow == 7
                                             Throw( "x7" )
                                          ENDIF
                                       END
                                       Mark( "a7" )
                                    ENDIF
                                 FINALLY
                                    Mark( "f6" )
                                    IF nMode == 3 .AND. nThrow == 6
                                       Throw( "x6" )
                                    ENDIF
                                 END
                                 Mark( "a6" )
                              ENDIF
                           CATCH e
                              Mark( "c5:" + Shown( e ) )
                              IF nMode == 2
                                 Throw( "r5" )
                              ENDIF
                           FINALLY
                              Mark( "f5" )
                              IF nMode == 3 .AND. nThrow == 5
                                 Throw( "x5" )
                              ENDIF
                           END
                           Mark( "a5" )
                        ENDIF
                     FINALLY
                        Mark( "f4" )
                        IF nMode == 3 .AND. nThrow == 4
                           Throw( "x4" )
                        ENDIF
                     END
                     Mark( "a4" )
                  ENDIF
               CATCH e
                  Mark( "c3:" + Shown( e ) )
                  IF nMode == 2
                     Throw( "r3" )
                  ENDIF
               FINALLY
                  Mark( "f3" )
                  IF nMode == 3 .AND. nThrow == 3
                     Throw( "x3" )
                  ENDIF
               END
               Mark( "a3" )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   CATCH e
      Mark( "c1:" + Shown( e ) )
      IF nMode == 2
         Throw( "r1" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE L8_170( nThrow, nMode )
   LOCAL e
   TRY
      Mark( "b1" )
      IF nThrow == 1
         Raise( 1, nMode )
      ELSE
         TRY
            Mark( "b2" )
            IF nThrow == 2
               Raise( 2, nMode )
            ELSE
               TRY
                  Mark( "b3" )
                  IF nThrow == 3
                     Raise( 3, nMode )
                  ELSE
                     TRY
                        Mark( "b4" )
                        IF nThrow == 4
                           Raise( 4, nMode )
                        ELSE
                           TRY
                              Mark( "b5" )
                              IF nThrow == 5
                                 Raise( 5, nMode )
                              ELSE
                                 TRY
                                    Mark( "b6" )
                                    IF nThrow == 6
                                       Raise( 6, nMode )
                                    ELSE
                                       TRY
                                          Mark( "b7" )
                                          IF nThrow == 7
                                             Raise( 7, nMode )
                                          ELSE
                                             TRY
                                                Mark( "b8" )
                                                IF nThrow == 8
                                                   Raise( 8, nMode )
                                                ENDIF
                                             CATCH e
                                                Mark( "c8:" + Shown( e ) )
                                                IF nMode == 2
                                                   Throw( "r8" )
                                                ENDIF
                                             FINALLY
                                                Mark( "f8" )
                                                IF nMode == 3 .AND. nThrow == 8
                                                   Throw( "x8" )
                                                ENDIF
                                             END
                                             Mark( "a8" )
                                          ENDIF
                                       FINALLY
                                          Mark( "f7" )
                                          IF nMode == 3 .AND. nThrow == 7
                                             Throw( "x7" )
                                          ENDIF
                                       END
                                       Mark( "a7" )
                                    ENDIF
                                 CATCH e
                                    Mark( "c6:" + Shown( e ) )
                                    IF nMode == 2
                                       Throw( "r6" )
                                    ENDIF
                                 FINALLY
                                    Mark( "f6" )
                                    IF nMode == 3 .AND. nThrow == 6
                                       Throw( "x6" )
                                    ENDIF
                                 END
                                 Mark( "a6" )
                              ENDIF
                           FINALLY
                              Mark( "f5" )
                              IF nMode == 3 .AND. nThrow == 5
                                 Throw( "x5" )
                              ENDIF
                           END
                           Mark( "a5" )
                        ENDIF
                     CATCH e
                        Mark( "c4:" + Shown( e ) )
                        IF nMode == 2
                           Throw( "r4" )
                        ENDIF
                     FINALLY
                        Mark( "f4" )
                        IF nMode == 3 .AND. nThrow == 4
                           Throw( "x4" )
                        ENDIF
                     END
                     Mark( "a4" )
                  ENDIF
               FINALLY
                  Mark( "f3" )
                  IF nMode == 3 .AND. nThrow == 3
                     Throw( "x3" )
                  ENDIF
               END
               Mark( "a3" )
            ENDIF
         CATCH e
            Mark( "c2:" + Shown( e ) )
            IF nMode == 2
               Throw( "r2" )
            ENDIF
         FINALLY
            Mark( "f2" )
            IF nMode == 3 .AND. nThrow == 2
               Throw( "x2" )
            ENDIF
         END
         Mark( "a2" )
      ENDIF
   FINALLY
      Mark( "f1" )
      IF nMode == 3 .AND. nThrow == 1
         Throw( "x1" )
      ENDIF
   END
RETURN

STATIC PROCEDURE Leaving()
   LOCAL i, k, c
   FOR EACH c IN { "EXIT", "LOOP", "RETURN" }
      FOR EACH k IN { 1, 2, 4, 6 }
         cLog := ""
         ? c, N( k ) + ":", Eval( LeaveBlock( c, k ) ), iif( SubStr( cLog, 2 ) == LeaveModel( c, k ), "ok", cLog )
      NEXT
   NEXT
RETURN

STATIC FUNCTION LeaveBlock( c, k )
   DO CASE
   CASE c == "EXIT" .AND. k == 1
      RETURN {|| LeaveExit1() }
   CASE c == "EXIT" .AND. k == 2
      RETURN {|| LeaveExit2() }
   CASE c == "EXIT" .AND. k == 4
      RETURN {|| LeaveExit4() }
   CASE c == "EXIT" .AND. k == 6
      RETURN {|| LeaveExit6() }
   CASE c == "LOOP" .AND. k == 1
      RETURN {|| LeaveLoop1() }
   CASE c == "LOOP" .AND. k == 2
      RETURN {|| LeaveLoop2() }
   CASE c == "LOOP" .AND. k == 4
      RETURN {|| LeaveLoop4() }
   CASE c == "LOOP" .AND. k == 6
      RETURN {|| LeaveLoop6() }
   CASE c == "RETURN" .AND. k == 1
      RETURN {|| LeaveReturn1() }
   CASE c == "RETURN" .AND. k == 2
      RETURN {|| LeaveReturn2() }
   CASE c == "RETURN" .AND. k == 4
      RETURN {|| LeaveReturn4() }
   CASE c == "RETURN" .AND. k == 6
      RETURN {|| LeaveReturn6() }
   ENDCASE
RETURN NIL

STATIC FUNCTION LeaveExit1()
   LOCAL i
   FOR i := 1 TO 2
      TRY
         Mark( "b1." + N( i ) )
         EXIT
      FINALLY
         Mark( "f1." + N( i ) )
      END
   NEXT
RETURN "ended"

STATIC FUNCTION LeaveExit2()
   LOCAL i
   FOR i := 1 TO 2
      TRY
         Mark( "b1." + N( i ) )
         TRY
            Mark( "b2." + N( i ) )
            EXIT
         FINALLY
            Mark( "f2." + N( i ) )
         END
      FINALLY
         Mark( "f1." + N( i ) )
      END
   NEXT
RETURN "ended"

STATIC FUNCTION LeaveExit4()
   LOCAL i
   FOR i := 1 TO 2
      TRY
         Mark( "b1." + N( i ) )
         TRY
            Mark( "b2." + N( i ) )
            TRY
               Mark( "b3." + N( i ) )
               TRY
                  Mark( "b4." + N( i ) )
                  EXIT
               FINALLY
                  Mark( "f4." + N( i ) )
               END
            FINALLY
               Mark( "f3." + N( i ) )
            END
         FINALLY
            Mark( "f2." + N( i ) )
         END
      FINALLY
         Mark( "f1." + N( i ) )
      END
   NEXT
RETURN "ended"

STATIC FUNCTION LeaveExit6()
   LOCAL i
   FOR i := 1 TO 2
      TRY
         Mark( "b1." + N( i ) )
         TRY
            Mark( "b2." + N( i ) )
            TRY
               Mark( "b3." + N( i ) )
               TRY
                  Mark( "b4." + N( i ) )
                  TRY
                     Mark( "b5." + N( i ) )
                     TRY
                        Mark( "b6." + N( i ) )
                        EXIT
                     FINALLY
                        Mark( "f6." + N( i ) )
                     END
                  FINALLY
                     Mark( "f5." + N( i ) )
                  END
               FINALLY
                  Mark( "f4." + N( i ) )
               END
            FINALLY
               Mark( "f3." + N( i ) )
            END
         FINALLY
            Mark( "f2." + N( i ) )
         END
      FINALLY
         Mark( "f1." + N( i ) )
      END
   NEXT
RETURN "ended"

STATIC FUNCTION LeaveLoop1()
   LOCAL i
   FOR i := 1 TO 2
      TRY
         Mark( "b1." + N( i ) )
         IF i == 1
            LOOP
         ENDIF
         EXIT
      FINALLY
         Mark( "f1." + N( i ) )
      END
   NEXT
RETURN "ended"

STATIC FUNCTION LeaveLoop2()
   LOCAL i
   FOR i := 1 TO 2
      TRY
         Mark( "b1." + N( i ) )
         TRY
            Mark( "b2." + N( i ) )
            IF i == 1
               LOOP
            ENDIF
            EXIT
         FINALLY
            Mark( "f2." + N( i ) )
         END
      FINALLY
         Mark( "f1." + N( i ) )
      END
   NEXT
RETURN "ended"

STATIC FUNCTION LeaveLoop4()
   LOCAL i
   FOR i := 1 TO 2
      TRY
         Mark( "b1." + N( i ) )
         TRY
            Mark( "b2." + N( i ) )
            TRY
               Mark( "b3." + N( i ) )
               TRY
                  Mark( "b4." + N( i ) )
                  IF i == 1
                     LOOP
                  ENDIF
                  EXIT
               FINALLY
                  Mark( "f4." + N( i ) )
               END
            FINALLY
               Mark( "f3." + N( i ) )
            END
         FINALLY
            Mark( "f2." + N( i ) )
         END
      FINALLY
         Mark( "f1." + N( i ) )
      END
   NEXT
RETURN "ended"

STATIC FUNCTION LeaveLoop6()
   LOCAL i
   FOR i := 1 TO 2
      TRY
         Mark( "b1." + N( i ) )
         TRY
            Mark( "b2." + N( i ) )
            TRY
               Mark( "b3." + N( i ) )
               TRY
                  Mark( "b4." + N( i ) )
                  TRY
                     Mark( "b5." + N( i ) )
                     TRY
                        Mark( "b6." + N( i ) )
                        IF i == 1
                           LOOP
                        ENDIF
                        EXIT
                     FINALLY
                        Mark( "f6." + N( i ) )
                     END
                  FINALLY
                     Mark( "f5." + N( i ) )
                  END
               FINALLY
                  Mark( "f4." + N( i ) )
               END
            FINALLY
               Mark( "f3." + N( i ) )
            END
         FINALLY
            Mark( "f2." + N( i ) )
         END
      FINALLY
         Mark( "f1." + N( i ) )
      END
   NEXT
RETURN "ended"

STATIC FUNCTION LeaveReturn1()
   LOCAL i
   FOR i := 1 TO 2
      TRY
         Mark( "b1." + N( i ) )
         RETURN "returned"
      FINALLY
         Mark( "f1." + N( i ) )
      END
   NEXT
RETURN "ended"

STATIC FUNCTION LeaveReturn2()
   LOCAL i
   FOR i := 1 TO 2
      TRY
         Mark( "b1." + N( i ) )
         TRY
            Mark( "b2." + N( i ) )
            RETURN "returned"
         FINALLY
            Mark( "f2." + N( i ) )
         END
      FINALLY
         Mark( "f1." + N( i ) )
      END
   NEXT
RETURN "ended"

STATIC FUNCTION LeaveReturn4()
   LOCAL i
   FOR i := 1 TO 2
      TRY
         Mark( "b1." + N( i ) )
         TRY
            Mark( "b2." + N( i ) )
            TRY
               Mark( "b3." + N( i ) )
               TRY
                  Mark( "b4." + N( i ) )
                  RETURN "returned"
               FINALLY
                  Mark( "f4." + N( i ) )
               END
            FINALLY
               Mark( "f3." + N( i ) )
            END
         FINALLY
            Mark( "f2." + N( i ) )
         END
      FINALLY
         Mark( "f1." + N( i ) )
      END
   NEXT
RETURN "ended"

STATIC FUNCTION LeaveReturn6()
   LOCAL i
   FOR i := 1 TO 2
      TRY
         Mark( "b1." + N( i ) )
         TRY
            Mark( "b2." + N( i ) )
            TRY
               Mark( "b3." + N( i ) )
               TRY
                  Mark( "b4." + N( i ) )
                  TRY
                     Mark( "b5." + N( i ) )
                     TRY
                        Mark( "b6." + N( i ) )
                        RETURN "returned"
                     FINALLY
                        Mark( "f6." + N( i ) )
                     END
                  FINALLY
                     Mark( "f5." + N( i ) )
                  END
               FINALLY
                  Mark( "f4." + N( i ) )
               END
            FINALLY
               Mark( "f3." + N( i ) )
            END
         FINALLY
            Mark( "f2." + N( i ) )
         END
      FINALLY
         Mark( "f1." + N( i ) )
      END
   NEXT
RETURN "ended"
