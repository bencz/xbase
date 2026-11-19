// Sets as hashes (each key its own value): two sets of 200,000 draws each from 0 .. 399,999, their union,
// intersection, difference and symmetric difference built as new sets; 2 rounds with new members.
STATIC s_nSeed := 777

PROCEDURE Main()
   LOCAL t := ElapsedMs(), r, i, k, hA, hB, hUnion, hInter, hDiff, hSym, nCheck := 0, ok
   FOR r := 1 TO 2
      hA := { => }
      hB := { => }
      FOR i := 1 TO 200000
         k := Rand( 400000 )
         hA[ k ] := k
         k := Rand( 400000 )
         hB[ k ] := k
      NEXT
      hUnion := { => }
      hInter := { => }
      hDiff := { => }
      hSym := { => }
      FOR EACH k IN hA
         hUnion[ k ] := k
         IF k $ hB
            hInter[ k ] := k
         ELSE
            hDiff[ k ] := k
            hSym[ k ] := k
         ENDIF
      NEXT
      FOR EACH k IN hB
         hUnion[ k ] := k
         IF !( k $ hA )
            hSym[ k ] := k
         ENDIF
      NEXT
      nCheck += Len( hA ) + Len( hB ) * 2 + Len( hUnion ) * 3 + Len( hInter ) * 5 + Len( hDiff ) * 7 + Len( hSym ) * 11
   NEXT
   ok := nCheck == 8609885 .AND. Len( hUnion ) == 252664 .AND. ;
      Len( hUnion ) == Len( hInter ) + Len( hSym ) .AND. Len( hA ) == Len( hInter ) + Len( hDiff )
   ? "set_operations", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

STATIC FUNCTION Rand( n )
   s_nSeed := ( s_nSeed * 16807 ) % 2147483647
   RETURN s_nSeed % n
