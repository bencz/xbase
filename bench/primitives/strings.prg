// The string primitives one by one: each runs N times, inline in its loop, on one of four strings chosen by the pass
// (so that no compiler moves the work out of the loop), and prints `<name> <nanoseconds per operation>`; strings.c does
// the same work in C, as a C program would write it (bytes in place, a buffer, memcpy), and the `primitives` target
// prints the two side by side. The sink keeps every result alive.
#define N 4000000
#xcommand MEASURE <name> AS <expr> => ;
   t := ElapsedMs() ;; FOR i := 1 TO N ;; x := <expr> ;; Keep( x ) ;; NEXT ;; Report( <name>, ElapsedMs() - t )
#xcommand MEASURE <name> DO <stmt> => ;
   t := ElapsedMs() ;; FOR i := 1 TO N ;; <stmt> ;; NEXT ;; Report( <name>, ElapsedMs() - t )

STATIC s_nSink := 0

PROCEDURE Main()
   LOCAL t, i, x, k, n, s
   LOCAL aS := { "The quick brown fox jumps over the lazy dog 0123456789", ;
                 "Pack my box with five dozen liquor jugs, then go home!!", ;
                 "Sphinx of black quartz, judge my vow: 42 times, again.", ;
                 "How vexingly quick daft zebras jump over 9876543210 hay" }
   LOCAL aPad := { "   padded words here   ", "  two  ", "none", "      six      " }
   LOCAL cLong := Replicate( "abcdefghij", 100 ) + "needle" + Replicate( "klmnopqrst", 100 )
   MEASURE "baseline" AS aS[ i % 4 + 1 ]
   MEASURE "len" AS Len( aS[ i % 4 + 1 ] )
   MEASURE "asc" AS Asc( aS[ i % 4 + 1 ] )
   MEASURE "chr" AS Chr( i % 256 )
   MEASURE "substr_1" AS SubStr( aS[ i % 4 + 1 ], i % 50 + 1, 1 )
   MEASURE "substr_1_dec" AS SubStr( aS[ i % 4 + 1 ], Int( i / 7 ) % 50 + 1, 1 )
   MEASURE "asc_substr" AS Asc( SubStr( aS[ i % 4 + 1 ], i % 50 + 1, 1 ) )
   MEASURE "substr_10" AS SubStr( aS[ i % 4 + 1 ], i % 40 + 1, 10 )
   MEASURE "left_10" AS Left( aS[ i % 4 + 1 ], 10 )
   MEASURE "right_10" AS Right( aS[ i % 4 + 1 ], 10 )
   MEASURE "at_char" AS At( "z", aS[ i % 4 + 1 ] )
   MEASURE "at_word" AS At( "over", aS[ i % 4 + 1 ] )
   MEASURE "at_long" AS At( "needle", cLong )
   MEASURE "rat_char" AS RAt( "o", aS[ i % 4 + 1 ] )
   MEASURE "contains" AS "jump" $ aS[ i % 4 + 1 ]
   MEASURE "upper" AS Upper( aS[ i % 4 + 1 ] )
   MEASURE "lower" AS Lower( aS[ i % 4 + 1 ] )
   MEASURE "alltrim" AS AllTrim( aPad[ i % 4 + 1 ] )
   MEASURE "ltrim" AS LTrim( aPad[ i % 4 + 1 ] )
   MEASURE "rtrim" AS RTrim( aPad[ i % 4 + 1 ] )
   MEASURE "padl_20" AS PadL( aPad[ i % 4 + 1 ], 30 )
   MEASURE "padr_20" AS PadR( aPad[ i % 4 + 1 ], 30 )
   MEASURE "padc_20" AS PadC( aPad[ i % 4 + 1 ], 30 )
   MEASURE "replicate_20" AS Replicate( aPad[ i % 4 + 1 ], 3 )
   MEASURE "space_20" AS Space( i % 4 + 20 )
   MEASURE "strtran" AS StrTran( aS[ i % 4 + 1 ], "o", "0" )
   MEASURE "stuff" AS Stuff( aS[ i % 4 + 1 ], 5, 5, "slow" )
   MEASURE "concat_2" AS aS[ i % 4 + 1 ] + aPad[ i % 4 + 1 ]
   MEASURE "concat_4_chars" AS Chr( i % 26 + 65 ) + Chr( i % 26 + 66 ) + Chr( i % 26 + 67 ) + Chr( i % 26 + 68 )
   MEASURE "eq_exact" AS aS[ i % 4 + 1 ] == aS[ ( i + 1 ) % 4 + 1 ]
   MEASURE "eq" AS aS[ i % 4 + 1 ] = "The quick"
   MEASURE "lt" AS aS[ i % 4 + 1 ] < aS[ ( i + 1 ) % 4 + 1 ]
   MEASURE "str_int" AS Str( i )
   MEASURE "str_dec" AS Str( i / 3, 12, 4 )
   MEASURE "val" AS Val( aS[ i % 4 + 1 ] )
   MEASURE "transform" AS Transform( i, "999,999,999" )
   MEASURE "isdigit" AS IsDigit( SubStr( aS[ i % 4 + 1 ], i % 50 + 1, 1 ) )
   MEASURE "empty" AS Empty( aPad[ i % 4 + 1 ] )
   s := ""
   MEASURE "append_1" DO s += Chr( i % 26 + 65 )
   s := ""
   MEASURE "append_4" DO s += aPad[ 3 ]
   s := ""
   MEASURE "append_concat_4" DO s += Chr( i % 26 + 65 ) + Chr( i % 26 + 66 ) + Chr( i % 26 + 67 ) + Chr( i % 26 + 68 )
   Keep( s )
   s := Replicate( aS[ 1 ], 1000 )
   n := 0
   t := ElapsedMs()
   FOR k := 1 TO N / Len( s ) * 4
      FOR i := 1 TO Len( s )
         n += Asc( SubStr( s, i, 1 ) )
      NEXT
   NEXT
   Report( "iterate_asc", ( ElapsedMs() - t ) / 4 )
   t := ElapsedMs()
   FOR k := 1 TO N / Len( s ) * 4
      FOR EACH x IN s
         n += Asc( x )
      NEXT
   NEXT
   Report( "iterate_foreach", ( ElapsedMs() - t ) / 4 )
   Keep( n )
   ? "sink", s_nSink
RETURN

STATIC PROCEDURE Keep( x )
   s_nSink += IIf( ValType( x ) == "C", Len( x ), IIf( ValType( x ) == "N", 1, IIf( x == .T., 1, 0 ) ) )
RETURN

STATIC PROCEDURE Report( cName, nMs )
   ? PadR( cName, 18 ), Str( nMs * 1000000 / N, 8, 1 )
RETURN
