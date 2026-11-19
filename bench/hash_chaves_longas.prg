// Hashes with long string keys: 300,000 keys of 82 to 131 bytes that share a 66-byte prefix (so equal hashes compare
// whole keys), inserted, then looked up through other strings of the same text, and 300,000 missing keys of the same
// shape looked up.
STATIC s_nSeed := 99

PROCEDURE Main()
   LOCAL t, h := { => }, aKeys := Array( 300000 ), aSame := Array( 300000 ), aMiss := Array( 300000 ), i, k, n, ok
   LOCAL nSum := 0, nMiss := 0, cPrefix := "https://example.com/catalog/products/category/electronics/item?id="
   FOR i := 1 TO Len( aKeys )
      n := Rand( 1000000 )
      aKeys[ i ] := cPrefix + StrZero( i, 7 ) + "&v=" + StrZero( n, 6 ) + Replicate( "x", i % 50 )
      aSame[ i ] := cPrefix + StrZero( i, 7 ) + "&v=" + StrZero( n, 6 ) + Replicate( "x", i % 50 )
      aMiss[ i ] := cPrefix + StrZero( i, 7 ) + "&w=" + StrZero( n, 6 ) + Replicate( "x", i % 50 )
   NEXT
   t := ElapsedMs()
   FOR i := 1 TO Len( aKeys )
      h[ aKeys[ i ] ] := i
   NEXT
   FOR i := Len( aSame ) TO 1 STEP -1
      nSum += h[ aSame[ i ] ]
   NEXT
   FOR EACH k IN aMiss
      IF !( k $ h )
         nMiss++
      ENDIF
   NEXT
   ok := Len( h ) == 300000 .AND. nSum == 45000150000 .AND. nMiss == 300000
   ? "hash_chaves_longas", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

STATIC FUNCTION Rand( n )
   s_nSeed := ( s_nSeed * 16807 ) % 2147483647
   RETURN s_nSeed % n
