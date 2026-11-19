// Hashes with many keys: 1,000,000 numeric keys drawn from a fixed-seed generator counted in one hash (about 632,000
// distinct), then 1,500,000 lookups of keys drawn from twice the range, about two thirds of them missing.
STATIC s_nSeed := 2024

PROCEDURE Main()
   LOCAL t := ElapsedMs(), h := { => }, i, k, nHits := 0, nSum := 0, ok
   FOR i := 1 TO 1000000
      k := Rand( 1000000 )
      IF k $ h
         h[ k ] += 1
      ELSE
         h[ k ] := 1
      ENDIF
   NEXT
   FOR i := 1 TO 1500000
      k := Rand( 2000000 )
      IF k $ h
         nHits++
         nSum += h[ k ]
      ENDIF
   NEXT
   ok := Len( h ) == 631967 .AND. nHits == 473472 .AND. nSum == 748930
   ? "hash_massivo", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

STATIC FUNCTION Rand( n )
   s_nSeed := ( s_nSeed * 16807 ) % 2147483647
   RETURN s_nSeed % n
